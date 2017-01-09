import subprocess
import time
import sys
import datetime
import platform
import os
import threading
import functools
import unittest
import json
import xmlrunner
import shutil

from testFunctions import read_line
from testFunctions import read_til_getShell
from testFunctions import kill_process
from testFunctions import exec_xshell_commands


############   Retrieve variables from configuration file    ##########################
class LOCALHOST:
    user =""
    password = ""
    host = ""
    xprotocol_port = ""
    port =""
class REMOTEHOST:
    user = ""
    password =""
    host = ""
    xprotocol_port = ""
    port = ""

if 'CONFIG_PATH' in os.environ and 'MYSQLX_PATH' in os.environ and os.path.isfile(os.environ['CONFIG_PATH']) and os.path.isfile(os.environ['MYSQLX_PATH']):
    # **** JENKINS EXECUTION ****
    config_path = os.environ['CONFIG_PATH']
    config=json.load(open(config_path))
    MYSQL_SHELL = os.environ['MYSQLX_PATH']
    Exec_files_location = os.environ['AUX_FILES_PATH']
    cluster_Path = os.environ['CLUSTER_PATH']
    XSHELL_QA_TEST_ROOT = os.environ['XSHELL_QA_TEST_ROOT']
    XMLReportFilePath = XSHELL_QA_TEST_ROOT+"/adminapi_qa_test.xml"
else:
    # **** LOCAL EXECUTION ****
    config=json.load(open('config_local.json'))
    MYSQL_SHELL = str(config["general"]["xshell_path"])
    Exec_files_location = str(config["general"]["aux_files_path"])
    cluster_Path = str(config["general"]["cluster_path"])
    XMLReportFilePath = "adminapi_qa_test.xml"

#########################################################################

LOCALHOST.user = str(config["local"]["user"])
LOCALHOST.password = str(config["local"]["password"])
LOCALHOST.host = str(config["local"]["host"])
LOCALHOST.xprotocol_port = str(config["local"]["xprotocol_port"])
LOCALHOST.port = str(config["local"]["port"])

REMOTEHOST.user = str(config["remote"]["user"])
REMOTEHOST.password = str(config["remote"]["password"])
REMOTEHOST.host = str(config["remote"]["host"])
REMOTEHOST.xprotocol_port = str(config["remote"]["xprotocol_port"])
REMOTEHOST.port = str(config["remote"]["port"])



class globalvar:
    last_found=""
    last_search=""

###########################################################################################

class XShell_TestCases(unittest.TestCase):


  def test_MYS_900(self):
      '''MYS-900 REMOVED SEED NODE SHOWN AS OFFLINE CLUSTER MEMBER'''
      instances = ["3310", "3320", "3330"]
      kill_process("3310",cluster_Path, MYSQL_SHELL)
      kill_process("3320",cluster_Path, MYSQL_SHELL)
      kill_process("3330",cluster_Path, MYSQL_SHELL)
      default_sandbox_path = "/mysql-sandboxes"

      results = 'PASS'
      init_command = [MYSQL_SHELL, '--interactive=full', '--passwords-from-stdin']
      x_cmds = [("dba.deploySandboxInstance(3310, {password: \"" + LOCALHOST.password + "\"})\n",
                 "Instance localhost:3310 successfully deployed and started."),
                ("dba.deploySandboxInstance(3320, {password: \"" + LOCALHOST.password + "\"})\n",
                 "Instance localhost:3320 successfully deployed and started."),
                ("dba.deploySandboxInstance(3330, {password: \"" + LOCALHOST.password + "\"})\n",
                 "Instance localhost:3330 successfully deployed and started."),
                ("\connect root:" + LOCALHOST.password + "@localhost:3310\n",
                 'Classic Session successfully established. No default schema selected.'),
                ("myCluster = dba.createCluster(\"myCluster\")\n",
                 'Cluster successfully created'),
                ("myCluster.addInstance(\"root:" + LOCALHOST.password + "@localhost:3320\")\n",
                 'The instance \'root@localhost:3320\' was successfully added to the cluster'),
                ("myCluster.addInstance(\"root:" + LOCALHOST.password + "@localhost:3330\")\n",
                 'The instance \'root@localhost:3330\' was successfully added to the cluster'),
                ("myCluster.removeInstance('localhost:3310')\n",
                 "The instance 'localhost:3310' was successfully removed from the cluster"),
                ("\connect root:" + LOCALHOST.password + "@localhost:3320\n",
                 'Classic Session successfully established. No default schema selected.'),
                ("myCluster.status()\n",
                 'root@localhost:3310')
                ]
      results = exec_xshell_commands(init_command, x_cmds)

      if results.find("localhost:3310") == -1:
          results = 'PASS'

      kill_process("3310",cluster_Path, MYSQL_SHELL)
      kill_process("3320",cluster_Path, MYSQL_SHELL)
      kill_process("3330",cluster_Path, MYSQL_SHELL)

      self.assertEqual(results, 'PASS')

      # Destroy the cluster
      #cleanup_instances(["3310", "3320", "3330"])

  # ----------------------------------------------------------------------
#
# if __name__ == '__main__':
#     unittest.main()

if __name__ == '__main__':
  unittest.main( testRunner=xmlrunner.XMLTestRunner(file(XMLReportFilePath,"w")))