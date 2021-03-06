shell.connect(__uripwd);
session.drop_schema('prepared_stmt');
schema = session.create_schema('prepared_stmt');
collection = schema.create_collection('test_collection');

collection.add({'_id': '001', 'name': 'george', 'age': 18}).execute();
collection.add({'_id': '002', 'name': 'james', 'age': 17}).execute();
collection.add({'_id': '003', 'name': 'luke', 'age': 18}).execute();
collection.add({'_id': '004', 'name': 'mark', 'age': 21}).execute();


#@ First execution is normal
crud = collection.find();
crud.execute();

#@ Second execution attempts preparing statement, disables prepared statements on session, executes normal
crud.execute()

#@ Third execution does normal execution
crud.execute()

#@ remove() first call
crud = collection.remove('_id = :id');
crud.bind('id', '004').execute();
collection.find();

#@ remove() second call, no prepare
crud.bind('id', '003').execute();
collection.find();

#@ modify() first call
crud = collection.modify('_id = :id').set('age', 20);
crud.bind('id', '001').execute();
collection.find();

#@ modify() second call, no prepare
crud.bind('id', '002').execute();
collection.find();

#@ sql() first call
sql = session.sql('select * from prepared_stmt.test_collection');
sql.execute();

#@ sql() second call, no prepare
sql.execute();

#@<> Finalizing
session.drop_schema('prepared_stmt');
session.close();
