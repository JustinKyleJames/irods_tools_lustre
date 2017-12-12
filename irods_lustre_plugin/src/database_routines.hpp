#ifndef _LUSTRE_IRODS_API_DB_ROUTINES
#define _LUSTRE_IRODS_API_DB_ROUTINES

#include "icatStructs.hpp"

int cmlExecuteNoAnswerSql( const char *sql, icatSessionStruct *icss );
int cmlGetStringValueFromSql( const char *sql, char *cVal, int cValSize, std::vector<std::string> &bindVars, icatSessionStruct *icss );
rodsLong_t cmlGetCurrentSeqVal( icatSessionStruct *icss );
int cmlGetIntegerValueFromSql( const char *sql, rodsLong_t *iVal, std::vector<std::string> &bindVars, icatSessionStruct *icss );
int cmlClose( icatSessionStruct *icss );

#endif

