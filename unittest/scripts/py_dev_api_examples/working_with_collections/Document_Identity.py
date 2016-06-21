# If the _id is provided, it will be honored
result = myColl.add( { '_id': 'custom_id', 'a' : 1 } ).execute()
print "User Provided Id: %s" % result.get_last_document_id()

# If the _id is not provided, one will be automatically assigned
result = myColl.add( { 'b': 2 } ).execute()
print "Autogenerated Id: %s" % result.get_last_document_id()

