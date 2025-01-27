#define NACL_LOG_MODULE_NAME "ripple_ledger_service"

#include <string.h>

#include "native_client/src/trusted/ripple_ledger_service/ripple_ledger_service.h"

#include "native_client/src/public/ripple_ledger_service.h"
#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/shared/platform/nacl_sync.h"
#include "native_client/src/shared/platform/nacl_sync_checked.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "native_client/src/trusted/desc/nacl_desc_io.h"
#include "native_client/src/trusted/desc_cacheability/desc_cacheability.h"
#include "native_client/src/trusted/reverse_service/ripple_ledger_rpc.h"
#include "native_client/src/trusted/reverse_service/reverse_control_rpc.h"
#include "native_client/src/trusted/service_runtime/include/sys/errno.h"
#include "native_client/src/trusted/service_runtime/include/sys/fcntl.h"
#include "native_client/src/trusted/service_runtime/nacl_secure_service.h"
#include "native_client/src/trusted/validator/nacl_file_info.h"
#include "native_client/src/trusted/validator/rich_file_info.h"
#include "native_client/src/trusted/validator/validation_cache.h"

static void NaClRippleLedgerWaitForChannel_yield_mu(
    struct NaClRippleLedgerServiceConnection *self) {
  NaClLog(4, "Entered NaClRippleLedgerWaitForChannel_yield_mu\n");
  NaClXMutexLock(&self->mu);
  NaClLog(4, "NaClRippleLedgerWaitForChannel_yield_mu: checking channel\n");
  while (!self->channel_initialized) {
    NaClLog(4, "NaClRippleLedgerWaitForChannel_yield_mu: waiting\n");
    NaClXCondVarWait(&self->cv, &self->mu);
  }
  NaClLog(4, "Leaving NaClRippleLedgerWaitForChannel_yield_mu\n");
}

static void NaClRippleLedgerReleaseChannel_release_mu(
    struct NaClRippleLedgerServiceConnection *self) {
  NaClLog(4, "NaClRippleLedgerReleaseChannel_release_mu\n");
  NaClXMutexUnlock(&self->mu);
}

static void NaClRippleLedgerServiceGetAccountTxsRpc(
    struct NaClSrpcRpc      *rpc,
    struct NaClSrpcArg      **in_args,
    struct NaClSrpcArg      **out_args,
    struct NaClSrpcClosure  *done_cls) {
  struct NaClRippleLedgerServiceConnection  *proxy_conn =
      (struct NaClRippleLedgerServiceConnection *) rpc->channel->server_instance_data;
  char  *account          = in_args[0]->arrays.str;
  int   ledger_index_min  = in_args[1]->u.ival;
  int   ledger_index_max  = in_args[2]->u.ival;
  char  *callback         = in_args[3]->arrays.str;
  NaClSrpcError srpc_error;

  UNREFERENCED_PARAMETER(out_args);
  NaClLog(4, "NaClRippleLedgerServiceGetAccountTxsRpc\n");

  NaClRippleLedgerWaitForChannel_yield_mu(proxy_conn);

  NaClLog(4,
          "NaClRippleLedgerServiceGetAccountTxsRpc: invoking %s\n",
          NACL_RIPPLE_LEDGER_GET_ACCOUNT_TXS);

  if (NACL_SRPC_RESULT_OK !=
      (srpc_error =
       NaClSrpcInvokeBySignature(&proxy_conn->client_channel,
                                 NACL_RIPPLE_LEDGER_GET_ACCOUNT_TXS,
                                 account,
                                 ledger_index_min,
                                 ledger_index_max,
                                 callback))) {
    NaClLog(LOG_ERROR,
            ("Ripple ledger read via channel 0x%"NACL_PRIxPTR" with RPC "
             NACL_RIPPLE_LEDGER_GET_ACCOUNT_TXS" failed: %d\n"),
            (uintptr_t) &proxy_conn->client_channel,
            srpc_error);
    rpc->result = srpc_error;
  } else {
    NaClLog(3,
            "NaClRippleLedgerServiceGetAccountTxsRpc, proxy returned\n");
    rpc->result = NACL_SRPC_RESULT_OK;
  }
  (*done_cls->Run)(done_cls);
  NaClRippleLedgerReleaseChannel_release_mu(proxy_conn);
}

static void NaClRippleLedgerServiceSubmitPaymentTxRpc(
    struct NaClSrpcRpc      *rpc,
    struct NaClSrpcArg      **in_args,
    struct NaClSrpcArg      **out_args,
    struct NaClSrpcClosure  *done_cls) {
  struct NaClRippleLedgerServiceConnection  *proxy_conn =
      (struct NaClRippleLedgerServiceConnection *) rpc->channel->server_instance_data;
  char  *account   = in_args[0]->arrays.str;
  char  *secret    = in_args[1]->arrays.str;
  char  *recipient = in_args[2]->arrays.str;
  char  *amount    = in_args[3]->arrays.str;
  char  *currency  = in_args[4]->arrays.str;
  char  *issuer    = in_args[5]->arrays.str;
  char  *callback  = in_args[6]->arrays.str;
  NaClSrpcError srpc_error;

  UNREFERENCED_PARAMETER(out_args);
  NaClLog(4, "NaClRippleLedgerServiceSubmitPaymentTxRpc\n");

  NaClRippleLedgerWaitForChannel_yield_mu(proxy_conn);

  NaClLog(4,
          "NaClRippleLedgerServiceSubmitPaymentTxRpc: invoking %s\n",
          NACL_RIPPLE_LEDGER_SUBMIT_PAYMENT_TX);

  if (NACL_SRPC_RESULT_OK !=
      (srpc_error =
       NaClSrpcInvokeBySignature(&proxy_conn->client_channel,
                                 NACL_RIPPLE_LEDGER_SUBMIT_PAYMENT_TX,
                                 account,
                                 secret,
                                 recipient,
                                 amount,
                                 currency,
                                 issuer,
                                 callback))) {
    NaClLog(LOG_ERROR,
            ("Ripple ledger read via channel 0x%"NACL_PRIxPTR" with RPC "
             NACL_RIPPLE_LEDGER_SUBMIT_PAYMENT_TX" failed: %d\n"),
            (uintptr_t) &proxy_conn->client_channel,
            srpc_error);
    rpc->result = srpc_error;
  } else {
    NaClLog(3,
            "NaClRippleLedgerServiceSubmitPaymentTxRpc, proxy returned\n");
    rpc->result = NACL_SRPC_RESULT_OK;
  }
  (*done_cls->Run)(done_cls);
  NaClRippleLedgerReleaseChannel_release_mu(proxy_conn);
}

struct NaClSrpcHandlerDesc const kNaClRippleLedgerServiceHandlers[] = {
  { NACL_RIPPLE_LEDGER_SERVICE_GET_ACCOUNT_TXS, NaClRippleLedgerServiceGetAccountTxsRpc, },
  { NACL_RIPPLE_LEDGER_SERVICE_SUBMIT_PAYMENT_TX, NaClRippleLedgerServiceSubmitPaymentTxRpc, },
  { (char const *) NULL, (NaClSrpcMethod) NULL, },
};


int NaClRippleLedgerServiceCtor(struct NaClRippleLedgerService  *self,
                                NaClThreadIfFactoryFunction     thread_factory_fn,
                                void                            *thread_factory_data,
                                struct NaClSecureService        *server) {
  NaClLog(4,
          ("Entered NaClRippleLedgerServiceCtor: self 0x%"NACL_PRIxPTR
           ", client 0x%"NACL_PRIxPTR"\n"),
          (uintptr_t) self,
          (uintptr_t) server);
  if (!NaClSimpleServiceCtor(&self->base,
                             kNaClRippleLedgerServiceHandlers,
                             thread_factory_fn,
                             thread_factory_data)) {
    return 0;
  }
  self->server = (struct NaClSecureService *)
      NaClRefCountRef((struct NaClRefCount *) server);
  NACL_VTBL(NaClRefCount, self) =
      (struct NaClRefCountVtbl *) &kNaClRippleLedgerServiceVtbl;
  return 1;
}

static void NaClRippleLedgerServiceDtor(struct NaClRefCount *vself) {
  struct NaClRippleLedgerService *self =
      (struct NaClRippleLedgerService *) vself;

  NaClRefCountUnref((struct NaClRefCount *) self->server);

  NACL_VTBL(NaClRefCount, self) =
      (struct NaClRefCountVtbl *) &kNaClSimpleServiceVtbl;
  (*NACL_VTBL(NaClRefCount, self)->Dtor)(vself);
}

int NaClRippleLedgerServiceConnectionCtor(struct NaClRippleLedgerServiceConnection  *self,
                                          struct NaClRippleLedgerService            *server,
                                          struct NaClDesc                           *conn) {
  NaClLog(4,
          "Entered NaClRippleLedgerServiceConnectionCtor, self 0x%"NACL_PRIxPTR"\n",
          (uintptr_t) self);
  if (!NaClSimpleServiceConnectionCtor(
          &self->base,
          (struct NaClSimpleService *) server,
          conn,
          (void *) self)) {
    NaClLog(4,
            ("NaClRippleLedgerServiceConnectionCtor: base class ctor"
             " NaClRippleLedgerServiceConnectionCtor failed\n"));
    return 0;
  }
  NaClXMutexCtor(&self->mu);
  NaClXCondVarCtor(&self->cv);
  self->channel_initialized = 0;
  NACL_VTBL(NaClRefCount, self) =
      (struct NaClRefCountVtbl *) &kNaClRippleLedgerServiceConnectionVtbl;
  return 1;
}

void NaClRippleLedgerServiceConnectionRevHandleConnect(
    struct NaClRippleLedgerServiceConnection  *self,
    struct NaClDesc                           *rev) {
  NaClLog(4, "Entered NaClRippleLedgerServiceConnectionRevHandleConnect\n");
  NaClXMutexLock(&self->mu);
  if (self->channel_initialized) {
    NaClLog(LOG_FATAL,
            "NaClRippleLedgerServiceConnectionRevHandleConnect: double connect?\n");
  }
  /*
   * If NaClSrpcClientCtor proves to take too long, we should spin off
   * another thread to do the initialization so that the reverse
   * client can accept additional reverse channels.
   */
  NaClLog(4,
          "NaClRippleLedgerServiceConnectionRevHandleConnect: Creating SrpcClient\n");
  if (NaClSrpcClientCtor(&self->client_channel, rev)) {
    NaClLog(4,
            ("NaClRippleLedgerServiceConnectionRevHandleConnect: SrpcClientCtor"
             " succeded, announcing.\n"));
    self->channel_initialized = 1;
    NaClXCondVarBroadcast(&self->cv);
    /* ownership of rev taken */
  } else {
    NaClLog(4,
            ("NaClRippleLedgerServiceConnectionRevHandleConnect: NaClSrpcClientCtor"
             " failed\n"));
  }
  NaClXMutexUnlock(&self->mu);
  NaClLog(4, "Leaving NaClRippleLedgerServiceConnectionRevHandleConnect\n");
}

static void NaClRippleLedgerServiceConnectionDtor(struct NaClRefCount *vself) {
  struct NaClRippleLedgerServiceConnection *self =
      (struct NaClRippleLedgerServiceConnection *) vself;
  NaClLog(4,
          "Entered NaClRippleLedgerServiceConnectionDtor: self 0x%"NACL_PRIxPTR"\n",
          (uintptr_t) self);
  NaClXMutexLock(&self->mu);
  while (!self->channel_initialized) {
    NaClLog(4,
            "NaClRippleLedgerServiceConnectionDtor:"
            " waiting for connection initialization\n");
    NaClXCondVarWait(&self->cv, &self->mu);
  }
  NaClXMutexUnlock(&self->mu);

  NaClLog(4, "NaClRippleLedgerServiceConnectionDtor: dtoring\n");

  NaClCondVarDtor(&self->cv);
  NaClMutexDtor(&self->mu);

  NaClSrpcDtor(&self->client_channel);
  NACL_VTBL(NaClSimpleServiceConnection, self) =
      &kNaClSimpleServiceConnectionVtbl;
  (*NACL_VTBL(NaClRefCount, self)->Dtor)(vself);
}

/*
 * NaClRippleLedgerServiceConnection is a NaClSimpleServiceConnection
 */
struct NaClSimpleServiceConnectionVtbl
  const kNaClRippleLedgerServiceConnectionVtbl = {
  {
    NaClRippleLedgerServiceConnectionDtor,
  },
  NaClSimpleServiceConnectionServerLoop,
};

static void NaClRippleLedgerReverseClientCallback(
    void                        *state,
    struct NaClThreadInterface  *tif,
    struct NaClDesc             *new_conn) {
  struct NaClRippleLedgerServiceConnection *mconn =
      (struct NaClRippleLedgerServiceConnection *) state;

  UNREFERENCED_PARAMETER(tif);
  NaClLog(4, "Entered NaClRippleLedgerReverseClientCallback\n");
  NaClRippleLedgerServiceConnectionRevHandleConnect(mconn, new_conn);
}

int NaClRippleLedgerServiceConnectionFactory(
    struct NaClSimpleService            *vself,
    struct NaClDesc                     *conn,
    struct NaClSimpleServiceConnection  **out) {
  struct NaClRippleLedgerService            *self =
      (struct NaClRippleLedgerService *) vself;
  struct NaClRippleLedgerServiceConnection  *mconn;
  NaClSrpcError                             rpc_result;
  int                                       bool_status;

  NaClLog(4,
          ("Entered NaClRippleLedgerServiceConnectionFactory, self 0x%"NACL_PRIxPTR
           "\n"),
          (uintptr_t) self);
  mconn = (struct NaClRippleLedgerServiceConnection *) malloc(sizeof *mconn);
  if (NULL == mconn) {
    NaClLog(4, "NaClRippleLedgerServiceConnectionFactory: no memory\n");
    return -NACL_ABI_ENOMEM;
  }
  NaClLog(4, "NaClRippleLedgerServiceConnectionFactory: creating connection obj\n");
  if (!NaClRippleLedgerServiceConnectionCtor(mconn, self, conn)) {
    free(mconn);
    return -NACL_ABI_EIO;
  }

  /*
   * Construct via NaClSecureReverseClientCtor with a callback to
   * process the new reverse connection -- which should be stored in
   * the mconn object.
   *
   * Make reverse RPC to obtain a new reverse RPC connection.
   */
  NaClLog(4, "NaClRippleLedgerServiceConnectionFactory: locking reverse channel\n");
  NaClLog(4, "NaClRippleLedgerServiceConnectionFactory: client 0x%"NACL_PRIxPTR"\n",
          (uintptr_t) self->server);
  NaClXMutexLock(&self->server->mu);
  if (NACL_REVERSE_CHANNEL_INITIALIZED !=
      self->server->reverse_channel_initialization_state) {
    NaClLog(LOG_FATAL,
            "NaClRippleLedgerServiceConnectionFactory invoked w/o reverse channel\n");
  }
  NaClLog(4, "NaClRippleLedgerServiceConnectionFactory: inserting handler\n");
  if (!(*NACL_VTBL(NaClSecureReverseClient, self->server->reverse_client)->
        InsertHandler)(self->server->reverse_client,
                       NaClRippleLedgerReverseClientCallback,
                       (void *) mconn)) {
    NaClLog(LOG_FATAL,
            ("NaClRippleLedgerServiceConnectionFactory:"
             " NaClSecureReverseClientInsertHandler failed\n"));
  }
  /*
   * NaClSrpcInvokeBySignature(""); tell plugin to connect and create
   * a reverse channel
   */
  NaClLog(4,
          ("NaClRippleLedgerServiceConnectionFactory: making RPC"
           " to set up connection\n"));
  rpc_result = NaClSrpcInvokeBySignature(&self->server->reverse_channel,
                                         NACL_REVERSE_CONTROL_ADD_CHANNEL,
                                         &bool_status);
  if (NACL_SRPC_RESULT_OK != rpc_result) {
    NaClLog(LOG_FATAL,
            "NaClRippleLedgerServiceConnectionFactory: add channel RPC failed: %d",
            rpc_result);
  }
  NaClLog(4,
          "NaClRippleLedgerServiceConnectionFactory: Start status %d\n",
          bool_status);

  NaClXMutexUnlock(&self->server->mu);

  *out = (struct NaClSimpleServiceConnection *) mconn;
  return 0;
}

struct NaClSimpleServiceVtbl const kNaClRippleLedgerServiceVtbl = {
  {
    NaClRippleLedgerServiceDtor,
  },
  NaClRippleLedgerServiceConnectionFactory,
  /* see name_service.c vtbl for connection factory and ownership */
  /*
   * The NaClRippleLedgerServiceConnectionFactory creates a subclass of a
   * NaClSimpleServiceConnectionFactory object that uses the reverse
   * connection object self->server to obtain a new RPC channel
   * with each ripple ledgerservice connection.
   */
  NaClSimpleServiceAcceptConnection,
  NaClSimpleServiceAcceptAndSpawnHandler,
  NaClSimpleServiceRpcHandler,
};
