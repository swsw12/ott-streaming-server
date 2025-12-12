/*
 * Minimal libpq-fe.h for Windows MSVC
 * Subset of PostgreSQL libpq interface needed for OTT server
 */

#ifndef LIBPQ_FE_H
#define LIBPQ_FE_H

#ifdef __cplusplus
extern "C" {
#endif

/* Connection status */
typedef enum {
    CONNECTION_OK,
    CONNECTION_BAD
} ConnStatusType;

/* Execution status */
typedef enum {
    PGRES_EMPTY_QUERY = 0,
    PGRES_COMMAND_OK,
    PGRES_TUPLES_OK,
    PGRES_COPY_OUT,
    PGRES_COPY_IN,
    PGRES_BAD_RESPONSE,
    PGRES_NONFATAL_ERROR,
    PGRES_FATAL_ERROR
} ExecStatusType;

/* Opaque types */
typedef struct pg_conn PGconn;
typedef struct pg_result PGresult;

/* Connection functions */
__declspec(dllimport) PGconn* PQconnectdb(const char *conninfo);
__declspec(dllimport) void PQfinish(PGconn *conn);
__declspec(dllimport) ConnStatusType PQstatus(const PGconn *conn);
__declspec(dllimport) char* PQerrorMessage(const PGconn *conn);

/* Query functions */
__declspec(dllimport) PGresult* PQexec(PGconn *conn, const char *query);
__declspec(dllimport) PGresult* PQexecParams(PGconn *conn,
    const char *command,
    int nParams,
    const unsigned int *paramTypes,
    const char *const *paramValues,
    const int *paramLengths,
    const int *paramFormats,
    int resultFormat);

/* Result functions */
__declspec(dllimport) ExecStatusType PQresultStatus(const PGresult *res);
__declspec(dllimport) char* PQresultErrorMessage(const PGresult *res);
__declspec(dllimport) int PQntuples(const PGresult *res);
__declspec(dllimport) int PQnfields(const PGresult *res);
__declspec(dllimport) char* PQgetvalue(const PGresult *res, int tup_num, int field_num);
__declspec(dllimport) int PQgetisnull(const PGresult *res, int tup_num, int field_num);
__declspec(dllimport) void PQclear(PGresult *res);

#ifdef __cplusplus
}
#endif

#endif /* LIBPQ_FE_H */
