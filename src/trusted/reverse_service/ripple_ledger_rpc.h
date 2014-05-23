#ifndef NATIVE_CLIENT_SRC_TRUSTED_REVERSE_SERVICE_RIPPLE_LEDGER_RPC_H_
#define NATIVE_CLIENT_SRC_TRUSTED_REVERSE_SERVICE_RIPPLE_LEDGER_RPC_H_

/*
 * Ripple ledger RPC signatures, for use by C and C++ code.
 */

#define NACL_RIPPLE_LEDGER_READ  "ripple_ledger_read:s:C"
/* ledger hash
   -> ledger data */
#define NACL_RIPPLE_LEDGER_GET_ACCOUNT_TXS  "ripple_get_account_txs:ss:"
/* account, ledger index */
 
#endif /* NATIVE_CLIENT_SRC_TRUSTED_REVERSE_SERVICE_RIPPLE_LEDGER_RPC_H_ */
