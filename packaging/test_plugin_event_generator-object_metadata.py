import os
import sys
import shutil
import contextlib
import tempfile
import json
import os.path

from time import sleep

if sys.version_info >= (2, 7):
    import unittest
else:
    import unittest2 as unittest

from ..configuration import IrodsConfig
from ..controller import IrodsController
from .resource_suite import ResourceBase
from ..test.command import assert_command
from . import session
from .. import test
from .. import paths
from .. import lib
import ustrings

@contextlib.contextmanager
def object_event_handler_configured(arg=None):
    filename = paths.server_config_path()
    with lib.file_backed_up(filename):
        irods_config = IrodsConfig()
        irods_config.server_config['advanced_settings']['rule_engine_server_sleep_time_in_seconds'] = 1

        irods_config.server_config['plugin_configuration']['rule_engines'].insert(0,
            {
                "instance_name": "irods_rule_engine_plugin-event_handler-data_object_modified-instance",
                "plugin_name": "irods_rule_engine_plugin-event_handler-data_object_modified",
                'plugin_specific_configuration': {
                    "policies_to_invoke" : [
                        {
                            "active_policy_clauses" : ["post"],
                            "events" : ["put"],
                            "policy"    : "irods_policy_event_delegate_collection_metadata",
                            "configuration" : {
                                "policies_to_invoke" : [
                                    {
                                        "match_metadata" : {
                                            "attribute" : "irods::testing::attribute",
                                            "value"     : "irods::testing::value",
                                            "units"     : "irods::testing::units"
                                        },
                                        "policy"    : "irods_policy_testing_policy",
                                        "configuration" : {
                                        }

                                    }
                                ]
                            }
                        }
                    ]
                }
            }
        )


        irods_config.server_config['plugin_configuration']['rule_engines'].insert(0,
           {
                "instance_name": "irods_rule_engine_plugin-event_delegate-collection_metadata-instance",
                "plugin_name": "irods_rule_engine_plugin-event_delegate-collection_metadata",
                "plugin_specific_configuration": {
                    "log_errors" : "true"
                }
           }
        )

        irods_config.server_config['plugin_configuration']['rule_engines'].insert(0,
           {
                "instance_name": "irods_rule_engine_plugin-policy_engine-testing_policy-instance",
                "plugin_name": "irods_rule_engine_plugin-policy_engine-testing_policy",
                "plugin_specific_configuration": {
                    "log_errors" : "true"
                }
           }
        )

        irods_config.commit(irods_config.server_config, irods_config.server_config_path)

        IrodsController().restart()

        try:
            yield
        finally:
            pass



@contextlib.contextmanager
def metadata_event_handler_configured(arg=None):
    filename = paths.server_config_path()
    with lib.file_backed_up(filename):
        irods_config = IrodsConfig()

        irods_config.server_config['plugin_configuration']['rule_engines'].insert(0,
            {
                "instance_name": "irods_rule_engine_plugin-event_handler-metadata_modified-instance",
                "plugin_name": "irods_rule_engine_plugin-event_handler-metadata_modified",
                'plugin_specific_configuration': {
                    "policies_to_invoke" : [
                        {
                            "active_policy_clauses" : ["post"],
                            "events" : ["metadata"],
                            "policy"    : "irods_policy_event_delegate_collection_metadata",
                            "configuration" : {
                                "policies_to_invoke" : [
                                    {
                                        "match_metadata" : {
                                            "attribute" : "irods::testing::attribute",
                                            "value"     : "irods::testing::value",
                                            "units"     : "irods::testing::units"
                                        },
                                        "policy"    : "irods_policy_testing_policy",
                                        "configuration" : {
                                        }

                                    }
                                ]
                            }
                        }
                    ]
                }
            }
        )


        irods_config.server_config['plugin_configuration']['rule_engines'].insert(0,
           {
                "instance_name": "irods_rule_engine_plugin-event_delegate-collection_metadata-instance",
                "plugin_name": "irods_rule_engine_plugin-event_delegate-collection_metadata",
                "plugin_specific_configuration": {
                    "log_errors" : "true"
                }
           }
        )

        irods_config.server_config['plugin_configuration']['rule_engines'].insert(0,
           {
                "instance_name": "irods_rule_engine_plugin-policy_engine-testing_policy-instance",
                "plugin_name": "irods_rule_engine_plugin-policy_engine-testing_policy",
                "plugin_specific_configuration": {
                    "log_errors" : "true"
                }
           }
        )

        irods_config.commit(irods_config.server_config, irods_config.server_config_path)

        IrodsController().restart()

        try:
            yield
        finally:
            pass




class TestEventDelegateCollectionMetadata(ResourceBase, unittest.TestCase):
    def setUp(self):
        super(TestEventDelegateCollectionMetadata, self).setUp()

    def tearDown(self):
        super(TestEventDelegateCollectionMetadata, self).tearDown()


    def test_event_handler_object(self):
        with session.make_session_for_existing_admin() as admin_session:
            with object_event_handler_configured():
                try:
                    admin_session.assert_icommand('imeta set -C /tempZone/home/rods irods::testing::attribute irods::testing::value irods::testing::units')
                    filename = 'test_put_file'
                    lib.create_local_testfile(filename)
                    admin_session.assert_icommand('iput ' + filename)
                    admin_session.assert_icommand('imeta ls -d ' + filename, 'STDOUT_SINGLELINE', 'PUT')
                finally:
                    admin_session.assert_icommand('irm -f ' + filename)
                    admin_session.assert_icommand('imeta rm -C /tempZone/home/rods irods::testing::attribute irods::testing::value irods::testing::units')
                    admin_session.assert_icommand('iadmin rum')



    def test_event_handler_metadata(self):
        with session.make_session_for_existing_admin() as admin_session:
            admin_session.assert_icommand('imeta set -C /tempZone/home irods::testing::attribute irods::testing::value irods::testing::units')
            filename = 'test_put_file'
            lib.create_local_testfile(filename)
            admin_session.assert_icommand('iput ' + filename)

            with metadata_event_handler_configured():
                try:
                    admin_session.assert_icommand('imeta set -d ' + filename + ' a v u')
                    admin_session.assert_icommand('imeta ls -d ' + filename, 'STDOUT_SINGLELINE', 'METADATA')
                finally:
                    admin_session.assert_icommand('irm -f ' + filename)
                    admin_session.assert_icommand('iadmin rum')



