// =-=-=-=-=-=-=-
// irods includes
#include "apiHandler.hpp"
#include "irods_stacktrace.hpp"
#include "irods_server_api_call.hpp"
#include "irods_re_serialization.hpp"
#include "objStat.h"
#include "icatHighLevelRoutines.hpp"
#include "irods_virtual_path.hpp"
#include "miscServerFunct.hpp"
#include "irods_configuration_keywords.hpp"

#include "boost/lexical_cast.hpp"
#include "boost/filesystem.hpp"

#include "database_routines.hpp"

// =-=-=-=-=-=-=-
// stl includes
#include <sstream>
#include <string>
#include <iostream>
#include <vector>

// json header
//#include <jeayeson/jeayeson.hpp>

// capn proto
#pragma push_macro("LIST")
#undef LIST

#pragma push_macro("ERROR")
#undef ERROR

#include "../../lustre_irods_connector/src/change_table.capnp.h"
#include <capnp/message.h>
#include <capnp/serialize-packed.h>

#pragma pop_macro("LIST")
#pragma pop_macro("ERROR")

#include "inout_structs.h"
#include "database_routines.hpp"

//int cmlExecuteNoAnswerSql( const char *sql, icatSessionStruct *icss );
//int cmlGetStringValueFromSql( const char *sql, char *cVal, int cValSize, std::vector<std::string> &bindVars, icatSessionStruct *icss );


#define MAX_BIND_VARS 32000
extern const char *cllBindVars[MAX_BIND_VARS];
extern int cllBindVarCount;

const std::string fidstr_avu_key = "lustre_identifier";

const std::string update_data_size_sql = "update R_DATA_MAIN set data_size = ? where data_id = (select * from ("
                   "select R_DATA_MAIN.data_id "
                   "from R_DATA_MAIN "
                   "inner join R_OBJT_METAMAP on R_DATA_MAIN.data_id = R_OBJT_METAMAP.object_id "
                   "inner join R_META_MAIN on R_META_MAIN.meta_id = R_OBJT_METAMAP.meta_id "
                   "where R_META_MAIN.meta_attr_name = '" + fidstr_avu_key + "' and R_META_MAIN.meta_attr_value = ?)temp_table)";

const std::string update_data_object_for_rename_sql = "update R_DATA_MAIN set data_name = ?, data_path = ?, coll_id = (select * from ("
                   "select R_COLL_MAIN.coll_id "
                   "from R_COLL_MAIN "
                   "inner join R_OBJT_METAMAP on R_COLL_MAIN.coll_id = R_OBJT_METAMAP.object_id "
                   "inner join R_META_MAIN on R_META_MAIN.meta_id = R_OBJT_METAMAP.meta_id "
                   "where R_META_MAIN.meta_attr_name = '" + fidstr_avu_key + "' and R_META_MAIN.meta_attr_value = ?)temp_table)"
                   "where data_id = (select * from ("
                   "select R_DATA_MAIN.data_id "
                   "from R_DATA_MAIN "
                   "inner join R_OBJT_METAMAP on R_DATA_MAIN.data_id = R_OBJT_METAMAP.object_id "
                   "inner join R_META_MAIN on R_META_MAIN.meta_id = R_OBJT_METAMAP.meta_id "
                   "where R_META_MAIN.meta_attr_name = '" + fidstr_avu_key + "' and R_META_MAIN.meta_attr_value = ?)temp_table2)";

const std::string get_collection_path_from_fidstr_sql = "select R_COLL_MAIN.coll_name "
                   "from R_COLL_MAIN "
                   "inner join R_OBJT_METAMAP on R_COLL_MAIN.coll_id = R_OBJT_METAMAP.object_id "
                   "inner join R_META_MAIN on R_META_MAIN.meta_id = R_OBJT_METAMAP.meta_id "
                   "where R_META_MAIN.meta_attr_name = '" + fidstr_avu_key + "' and R_META_MAIN.meta_attr_value = ?";


const std::string update_collection_for_rename_sql = "update R_COLL_MAIN set coll_name = ?, parent_coll_name = ? "
                   "where coll_id = (select * from ("
                   "select R_COLL_MAIN.coll_id "
                   "from R_COLL_MAIN "
                   "inner join R_OBJT_METAMAP on R_COLL_MAIN.coll_id = R_OBJT_METAMAP.object_id "
                   "inner join R_META_MAIN on R_META_MAIN.meta_id = R_OBJT_METAMAP.meta_id "
                   "where R_META_MAIN.meta_attr_name = '" + fidstr_avu_key + "' and R_META_MAIN.meta_attr_value = ?)temp_table)";

const std::string remove_object_meta_sql = "delete from R_OBJT_METAMAP where object_id = (select * from ("
                   "select R_OBJT_METAMAP.object_id "
                   "from R_OBJT_METAMAP "
                   "inner join R_META_MAIN on R_META_MAIN.meta_id = R_OBJT_METAMAP.meta_id "
                   "where R_META_MAIN.meta_attr_name = '" + fidstr_avu_key + "' and R_META_MAIN.meta_attr_value = ?)temp_table)";

const std::string unlink_sql = "delete from R_DATA_MAIN where data_id = (select * from ("
                   "select R_DATA_MAIN.data_id "
                   "from R_DATA_MAIN "
                   "inner join R_OBJT_METAMAP on R_DATA_MAIN.data_id = R_OBJT_METAMAP.object_id "
                   "inner join R_META_MAIN on R_META_MAIN.meta_id = R_OBJT_METAMAP.meta_id "
                   "where R_META_MAIN.meta_attr_name = '" + fidstr_avu_key + "' and R_META_MAIN.meta_attr_value = ?)temp_table)";

const std::string rmdir_sql = "delete from R_COLL_MAIN where coll_id = (select * from ("
                   "select R_COLL_MAIN.coll_id "
                   "from R_COLL_MAIN "
                   "inner join R_OBJT_METAMAP on R_COLL_MAIN.coll_id = R_OBJT_METAMAP.object_id "
                   "inner join R_META_MAIN on R_META_MAIN.meta_id = R_OBJT_METAMAP.meta_id "
                   "where R_META_MAIN.meta_attr_name = '" + fidstr_avu_key + "' and R_META_MAIN.meta_attr_value = ?)temp_table)";

const std::string get_collection_id_sql = "select coll_id from R_COLL_MAIN where coll_name = ?";

const std::string insert_data_obj_sql = "insert into R_DATA_MAIN (data_id, coll_id, data_name, data_repl_num, data_type_name, "
                   "data_size, resc_name, data_path, data_owner_name, data_owner_zone, data_is_dirty, data_map_id, resc_id) "
                   "values (?, ?, ?, 0, 'generic', ?, 'EMPTY_RESC_NAME', ?, ?, ?, 0, 0, ?)";

const std::string insert_user_ownership_data_object_sql = "insert into R_OBJT_ACCESS (object_id, user_id, access_type_id) values (?, ?, 1200)";

const std::string get_user_id_sql = "select user_id from R_USER_MAIN where user_name = ?";


// Returns the path in irods for a file in lustre.  
// Precondition:  irods_path is a buffer of size MAX_NAME_LEN
int lustre_path_to_irods_path(const char *lustre_path, const char *lustre_root_path, 
        const char *register_path, char *irods_path) {

    // make sure the file is underneath the lustre_root_path
    if (strncmp(lustre_path, lustre_root_path, strlen(lustre_root_path)) != 0) {
        return -1;
    }

    snprintf(irods_path, MAX_NAME_LEN, "%s%s", register_path, lustre_path + strlen(lustre_root_path));

    return 0;
}

/*std::string read_string_value_from_json_map(const std::string& key, const json_map& m) {
    std::stringstream tmp;
    tmp << m.find(key)->second; 

    std::string returnVal = tmp.str();

    // remove quotes
    returnVal.erase(remove(returnVal.begin(), returnVal.end(), '\"' ), returnVal.end());

    return returnVal;
}*/


int call_irodsLustreApiInp_irodsLustreApiOut( irods::api_entry* _api, 
                            rsComm_t*  _comm,
                            irodsLustreApiInp_t* _inp, 
                            irodsLustreApiOut_t** _out ) {
    return _api->call_handler<
               irodsLustreApiInp_t*,
               irodsLustreApiOut_t** >(
                   _comm,
                   _inp,
                   _out );
}

#ifdef RODS_SERVER
static irods::error serialize_irodsLustreApiInp_ptr( boost::any _p, 
                                            irods::re_serialization::serialized_parameter_t& _out) {
    try {
        irodsLustreApiInp_t* tmp = boost::any_cast<irodsLustreApiInp_t*>(_p);
        if(tmp) {
            _out["buf"] = boost::lexical_cast<std::string>(tmp->buf);
        }
        else {
            _out["buf"] = "";
        }
    }
    catch ( std::exception& ) {
        return ERROR(
                INVALID_ANY_CAST,
                "failed to cast irodsLustreApiInp_t ptr" );
    }

    return SUCCESS();
} // serialize_irodsLustreApiInp_ptr

static irods::error serialize_irodsLustreApiOut_ptr_ptr( boost::any _p,
                                                irods::re_serialization::serialized_parameter_t& _out) {
    try {
        irodsLustreApiOut_t** tmp = boost::any_cast<irodsLustreApiOut_t**>(_p);
        if(tmp && *tmp ) {
            irodsLustreApiOut_t*  l = *tmp;
            _out["status"] = boost::lexical_cast<std::string>(l->status);
        }
        else {
            _out["status"] = -1;
        }
    }
    catch ( std::exception& ) {
        return ERROR(
                INVALID_ANY_CAST,
                "failed to cast irodsLustreApiOut_t ptr" );
    }

    return SUCCESS();
} // serialize_irodsLustreApiOut_ptr_ptr
#endif


#ifdef RODS_SERVER
    #define CALL_IRODS_LUSTRE_API_INP_OUT call_irodsLustreApiInp_irodsLustreApiOut 
#else
    #define CALL_IRODS_LUSTRE_API_INP_OUT NULL 
#endif

// =-=-=-=-=-=-=-
// api function to be referenced by the entry

int rs_handle_lustre_records( rsComm_t* _comm, irodsLustreApiInp_t* _inp, irodsLustreApiOut_t** _out ) {

    rodsLog( LOG_NOTICE, "Dynamic API - Lustre API" );

    int status;

    // Bulk request must be performed on an iCAT server.  If this is not the iCAT, forward this
    // request to it.
    rodsServerHost_t *rodsServerHost;
    status = getAndConnRcatHost(_comm, MASTER_RCAT, (const char*)nullptr, &rodsServerHost);
    if ( status < 0 ) {
        rodsLog(LOG_ERROR, "Error:  getAndConnRcatHost returned %d", status);
        return status;
    }

    if ( rodsServerHost->localFlag != LOCAL_HOST ) {
        rodsLog(LOG_NOTICE, "Bulk request received by catalog consumer.  Forwarding request to catalog provider.");
        status = procApiRequest(rodsServerHost->conn, 15001, _inp, nullptr, (void**)_out, nullptr);
        return status;
    }

    std::string svc_role;
    irods::error ret = get_catalog_service_role(svc_role);
    if(!ret.ok()) {
        irods::log(PASS(ret));
        return ret.code();
    }

    if (irods::CFG_SERVICE_ROLE_PROVIDER != svc_role) {
        rodsLog(LOG_ERROR, "Error:  Attempting bulk Lustre operations on a catalog consumer.  Must connect to catalog provider.");
        return CAT_NOT_OPEN;
    }

    icatSessionStruct *icss;
    status = chlGetRcs( &icss );
    if ( status < 0 || !icss ) {
        return CAT_NOT_OPEN;
    }


    ( *_out ) = ( irodsLustreApiOut_t* )malloc( sizeof( irodsLustreApiOut_t ) );
    ( *_out )->status = 0;

    //const kj::ArrayPtr<const capnp::word> array_ptr{ reinterpret_cast<const capnp::word*>(&(*std::begin(_inp->buf))), 
    //    reinterpret_cast<const capnp::word*>(&(*std::end(_inp->buf)))};
    const kj::ArrayPtr<const capnp::word> array_ptr{ reinterpret_cast<const capnp::word*>(&(*(_inp->buf))), 
        reinterpret_cast<const capnp::word*>(&(*(_inp->buf + _inp->buflen)))};
    capnp::FlatArrayMessageReader message(array_ptr);

    ChangeMap::Reader changeMap = message.getRoot<ChangeMap>();

    // get user_id from user_name
    rodsLong_t user_id;
    std::vector<std::string> bindVars;
    bindVars.push_back(_comm->clientUser.userName);
    status = cmlGetIntegerValueFromSql(get_user_id_sql.c_str(), &user_id, bindVars, icss );
    if (status != 0) {
       rodsLog(LOG_ERROR, "Error getting user_id for user %s.  Error is %i", _comm->clientUser.userName, status);
       return SYS_USER_RETRIEVE_ERR;
    }

    std::string lustre_root_path(changeMap.getLustreRootPath().cStr()); 
    std::string register_path(changeMap.getRegisterPath().cStr()); 
    int64_t resource_id = changeMap.getResourceId();

    for (ChangeDescriptor::Reader entry : changeMap.getEntries()) {

        const ChangeDescriptor::EventTypeEnum event_type = entry.getEventType();
        std::string fidstr(entry.getFidstr().cStr());
        std::string lustre_path(entry.getLustrePath().cStr());
        std::string object_name(entry.getObjectName().cStr());
        const ChangeDescriptor::ObjectTypeEnum object_type = entry.getObjectType();
        std::string parent_fidstr(entry.getParentFidstr().cStr());
        int64_t file_size = entry.getFileSize();

        // Handle changes in iRODS.  For efficiency many use lower level DB routines and do not trigger dynamic PEPs.

        if (event_type == ChangeDescriptor::EventTypeEnum::CREATE) {
            char irods_path[MAX_NAME_LEN];
            if (lustre_path_to_irods_path(lustre_path.c_str(), lustre_root_path.c_str(), register_path.c_str(), irods_path) < 0) {
                rodsLog(LOG_NOTICE, "Skipping entry because lustre_path [%s] is not within lustre_root_path [%s].",
                       lustre_path.c_str(), lustre_root_path.c_str()); 
                continue;
            }

            // register object
            
            int seq_no = cmlGetCurrentSeqVal(icss);
            std::string username = _comm->clientUser.userName;
            std::string zone = _comm->clientUser.rodsZone;
            rodsLog(LOG_NOTICE, "seq_no=%i username=%s zone=%s", seq_no, username.c_str(), zone.c_str());
            rodsLog(LOG_NOTICE, "object_name = %s", object_name.c_str());

            boost::filesystem::path p(irods_path);

            // get collection id
            rodsLong_t coll_id;
            std::vector<std::string> bindVars;
            bindVars.push_back(p.parent_path().string());
            status = cmlGetIntegerValueFromSql(get_collection_id_sql.c_str(), &coll_id, bindVars, icss );
            if (status != 0) {
                rodsLog(LOG_ERROR, "Error during registration object %s.  Error getting collection id for collection %s.  Error is %i", 
                        fidstr.c_str(), p.parent_path().string().c_str(),  status);
                continue;
            }

            // insert data object
            cllBindVars[0] = std::to_string(seq_no).c_str();
            cllBindVars[1] = std::to_string(coll_id).c_str();
            cllBindVars[2] = object_name.c_str();
            cllBindVars[3] = std::to_string(file_size).c_str();  
            cllBindVars[4] = lustre_path.c_str(); 
            cllBindVars[5] = _comm->clientUser.userName;
            cllBindVars[6] = _comm->clientUser.rodsZone;
            cllBindVars[7] = std::to_string(resource_id).c_str(); 
            cllBindVarCount = 8;
            status = cmlExecuteNoAnswerSql(insert_data_obj_sql.c_str(), icss);
            if (status != 0) {
                rodsLog(LOG_ERROR, "Error registering object %s.  Error is %i", fidstr.c_str(), status);
                continue;
            }

            status =  cmlExecuteNoAnswerSql("commit", icss);
            if (status != 0) {
                rodsLog(LOG_ERROR, "Error committing insertion of new data_object %s.  Error is %i", fidstr.c_str(), status);
                continue;
            } 

            // insert user ownership
            cllBindVars[0] = std::to_string(seq_no).c_str();
            cllBindVars[1] = std::to_string(user_id).c_str();
            cllBindVarCount = 2;
            status = cmlExecuteNoAnswerSql(insert_user_ownership_data_object_sql.c_str(), icss);
            if (status != 0) {
                rodsLog(LOG_ERROR, "Error adding onwership to object %s.  Error is %i", fidstr.c_str(), status);
                continue;
            }

            status =  cmlExecuteNoAnswerSql("commit", icss);
            if (status != 0) {
                rodsLog(LOG_ERROR, "Error committing ownership of new data_object %s.  Error is %i", fidstr.c_str(), status);
                continue;
            }
            // --------------------------
            
            /*dataObjInfo_t data_obj_info;
            memset(&data_obj_info, 0, sizeof(data_obj_info));
            strncpy(data_obj_info.objPath, irods_path, MAX_NAME_LEN);
            strncpy(data_obj_info.dataType, "generic", NAME_LEN);
            strncpy(data_obj_info.filePath, lustre_path.c_str(), MAX_NAME_LEN);
            data_obj_info.rescId = resource_id;
            data_obj_info.dataSize = file_size;

            status = chlRegDataObj(_comm, &data_obj_info);
            rodsLog(LOG_NOTICE, "Return value from chlRegDataObj = %i", status);
            if (status < 0) {
                rodsLog(LOG_ERROR, "Error registering object %s.  Error is %i", fidstr.c_str(), status);
                continue;
            }*/

            // add lustre_identifier metadata
            keyValPair_t reg_param;
            memset(&reg_param, 0, sizeof(reg_param));
            addKeyVal(&reg_param, fidstr_avu_key.c_str(), fidstr.c_str());
            status = chlAddAVUMetadata(_comm, 0, "-d", irods_path, fidstr_avu_key.c_str(), fidstr.c_str(), "");
            rodsLog(LOG_NOTICE, "Return value from chlAddAVUMetdata = %i", status);
            if (status < 0) {
                rodsLog(LOG_ERROR, "Error adding %s metadata to object %s.  Error is %i", fidstr_avu_key.c_str(), fidstr.c_str(), status);
                continue;
            }

        } else if (event_type == ChangeDescriptor::EventTypeEnum::MKDIR) {

            // TODO is it better to use the parent_fidstr to find the parent to avoid race conditions?

            char irods_path[MAX_NAME_LEN];
            if (lustre_path_to_irods_path(lustre_path.c_str(), lustre_root_path.c_str(), register_path.c_str(), irods_path) < 0) {
                rodsLog(LOG_NOTICE, "Skipping mkdir on lustre_path [%s] which is not within lustre_root_path [%s].",
                       lustre_path.c_str(), lustre_root_path.c_str());
                continue;
            }

            collInfo_t coll_info;
            memset(&coll_info, 0, sizeof(coll_info));
            strncpy(coll_info.collName, irods_path, MAX_NAME_LEN);

            // register object
            status = chlRegColl(_comm, &coll_info);
            rodsLog(LOG_NOTICE, "Return value from chlRegColl = %i", status);
            if (status < 0) {
                rodsLog(LOG_ERROR, "Error registering collection %s.  Error is %i", fidstr.c_str(), status);
                continue;
            } 

            // add lustre_identifier metadata
            keyValPair_t reg_param;
            memset(&reg_param, 0, sizeof(reg_param));
            addKeyVal(&reg_param, fidstr_avu_key.c_str(), fidstr.c_str());
            status = chlAddAVUMetadata(_comm, 0, "-C", irods_path, fidstr_avu_key.c_str(), fidstr.c_str(), "");
            rodsLog(LOG_NOTICE, "Return value from chlAddAVUMetdata = %i", status);
            if (status < 0) {
                rodsLog(LOG_ERROR, "Error adding %s metadata to object %s.  Error is %i", fidstr_avu_key.c_str(), fidstr.c_str(), status);
                continue;
            }

        } else if (event_type == ChangeDescriptor::EventTypeEnum::OTHER) {

            // read and update the file size

            cllBindVars[0] = std::to_string(file_size).c_str(); //file_size_str.c_str();
            cllBindVars[1] = fidstr.c_str(); 
            cllBindVarCount = 2;
            status = cmlExecuteNoAnswerSql(update_data_size_sql.c_str(), icss);

            if (status != 0) {
                rodsLog(LOG_ERROR, "Error updating data_object_size for data_object %s.  Error is %i", fidstr.c_str(), status);
                cmlExecuteNoAnswerSql("rollback", icss);
                continue;
            }

            status =  cmlExecuteNoAnswerSql("commit", icss);

            if (status != 0) {
                rodsLog(LOG_ERROR, "Error committing update to data_object_size for data_object %s.  Error is %i", fidstr.c_str(), status);
                continue;
            } 


        } else if (event_type == ChangeDescriptor::EventTypeEnum::RENAME and object_type == ChangeDescriptor::ObjectTypeEnum::FILE) {

            // update data_name, data_path, and coll_id
            cllBindVars[0] = object_name.c_str();
            cllBindVars[1] = lustre_path.c_str();
            cllBindVars[2] = parent_fidstr.c_str();
            cllBindVars[3] = fidstr.c_str();
            cllBindVarCount = 4;
            status = cmlExecuteNoAnswerSql(update_data_object_for_rename_sql.c_str(), icss);

            if (status != 0) {
                rodsLog(LOG_ERROR, "Error updating data object rename for data_object %s.  Error is %i", fidstr.c_str(), status);
                cmlExecuteNoAnswerSql("rollback", icss);
                continue;
            }

            status =  cmlExecuteNoAnswerSql("commit", icss);

            if (status != 0) {
                rodsLog(LOG_ERROR, "Error committing update to data object rename for data_object %s.  Error is %i", fidstr.c_str(), status);
                continue;
            }

        } else if (event_type == ChangeDescriptor::EventTypeEnum::RENAME and object_type == ChangeDescriptor::ObjectTypeEnum::DIR) {

            char parent_path[MAX_NAME_LEN];
            char collection_path[MAX_NAME_LEN];

            // get the parent's path - using parent's fidstr
            std::vector<std::string> bindVars;
            bindVars.push_back(parent_fidstr);
            status = cmlGetStringValueFromSql(get_collection_path_from_fidstr_sql.c_str(), parent_path, MAX_NAME_LEN, bindVars, icss);
            if (status != 0) {
                rodsLog(LOG_ERROR, "Error looking up parent collection for rename for collection %s.  Error is %i", fidstr.c_str(), status);
                cmlExecuteNoAnswerSql("rollback", icss);

                // TODO as a backup just use what was sent to use and split the path
                continue;
            }

            snprintf(collection_path, MAX_NAME_LEN, "%s%s%s", parent_path, irods::get_virtual_path_separator().c_str(), object_name.c_str());

            rodsLog(LOG_NOTICE, "collection path = %s\tparent_path = %s", collection_path, parent_path);
              

            // update coll_name, parent_coll_name, and coll_id
            cllBindVars[0] = collection_path;
            cllBindVars[1] = parent_path;
            cllBindVars[2] = fidstr.c_str();
            cllBindVarCount = 3;
            status = cmlExecuteNoAnswerSql(update_collection_for_rename_sql.c_str(), icss);

            if (status != 0) {
                rodsLog(LOG_ERROR, "Error updating collection object rename for collection %s.  Error is %i", fidstr.c_str(), status);
                cmlExecuteNoAnswerSql("rollback", icss);
                continue;
            }

            status =  cmlExecuteNoAnswerSql("commit", icss);

            if (status != 0) {
                rodsLog(LOG_ERROR, "Error committing update to collection rename for collection %s.  Error is %i", fidstr.c_str(), status);
                continue;
            }

        } else if (event_type == ChangeDescriptor::EventTypeEnum::UNLINK) {

            cllBindVars[0] = fidstr.c_str();
            cllBindVarCount = 1;
            status = cmlExecuteNoAnswerSql(unlink_sql.c_str(), icss);

            if (status != 0) {
                rodsLog(LOG_ERROR, "Error deleting data object %s.  Error is %i", fidstr.c_str(), status);
                cmlExecuteNoAnswerSql("rollback", icss);
                continue;
            }

            // delete the metadata on the data object 
            cllBindVars[0] = fidstr.c_str();
            cllBindVarCount = 1;
            status = cmlExecuteNoAnswerSql(remove_object_meta_sql.c_str(), icss);

            if (status != 0) {
                // Couldn't delete metadata.  Just log and continue.
                rodsLog(LOG_ERROR, "Error deleting metadata from data object %s.  Error is %i", fidstr.c_str(), status);
            }


            status =  cmlExecuteNoAnswerSql("commit", icss);

            if (status != 0) {
                rodsLog(LOG_ERROR, "Error committing delete for data object %s.  Error is %i", fidstr.c_str(), status);
                continue;
            }

        } else if (event_type == ChangeDescriptor::EventTypeEnum::RMDIR) {

            cllBindVars[0] = fidstr.c_str();
            cllBindVarCount = 1;
            status = cmlExecuteNoAnswerSql(rmdir_sql.c_str(), icss);

            if (status != 0) {
                rodsLog(LOG_ERROR, "Error deleting directory %s.  Error is %i", fidstr.c_str(), status);
                cmlExecuteNoAnswerSql("rollback", icss);
                continue;
            }

            // delete the metadata on the collection 
            cllBindVars[0] = fidstr.c_str();
            cllBindVarCount = 1;
            status = cmlExecuteNoAnswerSql(remove_object_meta_sql.c_str(), icss);

            if (status != 0) {
                // Couldn't delete metadata.  Just log and continue.
                rodsLog(LOG_ERROR, "Error deleting metadata from collection %s.  Error is %i", fidstr.c_str(), status);
            }

            status =  cmlExecuteNoAnswerSql("commit", icss);

            if (status != 0) {
                rodsLog(LOG_ERROR, "Error committing delete for collection %s.  Error is %i", fidstr.c_str(), status);
                continue;
            }

        }
    }

    status = cmlClose(icss);
    rodsLog(LOG_NOTICE, "cmlClose status = %d", status);

    rodsLog( LOG_NOTICE, "Dynamic API - DONE" );

    return 0;
}

extern "C" {
    // =-=-=-=-=-=-=-
    // factory function to provide instance of the plugin
    irods::api_entry* plugin_factory( const std::string&,     //_inst_name
                                      const std::string& ) { // _context
        // =-=-=-=-=-=-=-
        // create a api def object
        irods::apidef_t def = { 15001,             // api number
                                RODS_API_VERSION, // api version
                                NO_USER_AUTH,     // client auth
                                NO_USER_AUTH,     // proxy auth
                                "IrodsLustreApiInp_PI", 0, // in PI / bs flag
                                "IrodsLustreApiOut", 0, // out PI / bs flag
                                std::function<
                                    int( rsComm_t*,irodsLustreApiInp_t*,irodsLustreApiOut_t**)>(
                                        rs_handle_lustre_records), // operation
								"rs_handle_lustre_records",    // operation name
                                0,  // null clear fcn
                                (funcPtr)CALL_IRODS_LUSTRE_API_INP_OUT
                              };
        // =-=-=-=-=-=-=-
        // create an api object
        irods::api_entry* api = new irods::api_entry( def );

#ifdef RODS_SERVER
        irods::re_serialization::add_operation(
                typeid(irodsLustreApiInp_t*),
                serialize_irodsLustreApiInp_ptr );

        irods::re_serialization::add_operation(
                typeid(irodsLustreApiOut_t**),
                serialize_irodsLustreApiOut_ptr_ptr );
#endif // RODS_SERVER

        // =-=-=-=-=-=-=-
        // assign the pack struct key and value
        api->in_pack_key   = "IrodsLustreApiInp_PI";
        api->in_pack_value = IrodsLustreApiInp_PI;

        api->out_pack_key   = "IrodsLustreApiOut_PI";
        api->out_pack_value = IrodsLustreApiOut_PI;

        return api;

    } // plugin_factory

}; // extern "C"
