// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_SCRIPT_INTERPRETER_H
#define BITCOIN_SCRIPT_INTERPRETER_H

#include "script_error.h"
#include "primitives/transaction.h"

#include <vector>
#include <stdint.h>
#include <string>

#include "amount.h"

class CPubKey;
class COutPoint;
class CScript;
class CTransaction;
class CTxOut;
class uint256;

/** Signature hash types/flags */
enum
{
    SIGHASH_ALL = 1,
    SIGHASH_NONE = 2,
    SIGHASH_SINGLE = 3,
    SIGHASH_ANYONECANPAY = 0x80,
};

/** Script verification flags */
enum
{
    SCRIPT_VERIFY_NONE      = 0,

    // Evaluate P2SH subscripts (softfork safe, BIP16).
    SCRIPT_VERIFY_P2SH      = (1U << 0),

    // Passing a non-strict-DER signature or one with undefined hashtype to a checksig operation causes script failure.
    // Evaluating a pubkey that is not (0x04 + 64 bytes) or (0x02 or 0x03 + 32 bytes) by checksig causes script failure.
    // (softfork safe, but not used or intended as a consensus rule).
    SCRIPT_VERIFY_STRICTENC = (1U << 1),

    // Passing a non-strict-DER signature to a checksig operation causes script failure (softfork safe, BIP62 rule 1)
    SCRIPT_VERIFY_DERSIG    = (1U << 2),

    // Passing a non-strict-DER signature or one with S > order/2 to a checksig operation causes script failure
    // (softfork safe, BIP62 rule 5).
    SCRIPT_VERIFY_LOW_S     = (1U << 3),

    // verify dummy stack item consumed by CHECKMULTISIG is of zero-length (softfork safe, BIP62 rule 7).
    SCRIPT_VERIFY_NULLDUMMY = (1U << 4),

    // Using a non-push operator in the scriptSig causes script failure (softfork safe, BIP62 rule 2).
    SCRIPT_VERIFY_SIGPUSHONLY = (1U << 5),

    // Require minimal encodings for all push operations (OP_0... OP_16, OP_1NEGATE where possible, direct
    // pushes up to 75 bytes, OP_PUSHDATA up to 255 bytes, OP_PUSHDATA2 for anything larger). Evaluating
    // any other push causes the script to fail (BIP62 rule 3).
    // In addition, whenever a stack element is interpreted as a number, it must be of minimal length (BIP62 rule 4).
    // (softfork safe)
    SCRIPT_VERIFY_MINIMALDATA = (1U << 6),

    // Discourage use of NOPs reserved for upgrades (NOP1-10)
    //
    // Provided so that nodes can avoid accepting or mining transactions
    // containing executed NOP's whose meaning may change after a soft-fork,
    // thus rendering the script invalid; with this flag set executing
    // discouraged NOPs fails the script. This verification flag will never be
    // a mandatory flag applied to scripts in a block. NOPs that are not
    // executed, e.g.  within an unexecuted IF ENDIF block, are *not* rejected.
    SCRIPT_VERIFY_DISCOURAGE_UPGRADABLE_NOPS  = (1U << 7),

    // Verify CHECKLOCKTIMEVERIFY (BIP65)
    SCRIPT_VERIFY_CHECKLOCKTIMEVERIFY = (1U << 9),

    // support CHECKSEQUENCEVERIFY opcode
    SCRIPT_VERIFY_CHECKSEQUENCEVERIFY = (1U << 10),

    // Execute sidechain-related opcodes instead of treating them as NOPs
    SCRIPT_VERIFY_WITHDRAW = (1U << 11),

    // Dirty hack to require a higher bar of bitcoin block confirmation in mempool
    SCRIPT_VERIFY_INCREASE_CONFIRMATIONS_REQUIRED = (1U << 12)
};

uint256 SignatureHash(const CScript &scriptCode, const CTxOutValue& nValue, const CTransaction& txTo, unsigned int nIn, int nHashType);

class BaseSignatureChecker
{
public:
    virtual bool CheckSig(const std::vector<unsigned char>& scriptSig, const std::vector<unsigned char>& vchPubKey, const CScript& scriptCode) const
    {
        return false;
    }

    virtual bool CheckLockTime(const CScriptNum& nLockTime, bool fSequence = false) const
    {
         return false;
    }

    virtual CTxOut GetOutputOffsetFromCurrent(const int offset) const;
    virtual COutPoint GetPrevOut() const;

    virtual CTxOutValue GetValueIn() const
    {
        return -1;
    }

    virtual CTxOutValue GetValueInPrevIn() const
    {
        return -1;
    }

    virtual CAmount GetTransactionFee() const
    {
        return -1;
    }

#define FEDERATED_PEG_SIDECHAIN_ONLY
#ifdef FEDERATED_PEG_SIDECHAIN_ONLY
    virtual bool IsConfirmedBitcoinBlock(const uint256& hash, bool fConservativeConfirmationRequirements) const
    {
        return false;
    }
#endif

    virtual ~BaseSignatureChecker() {}
};

class TransactionNoWithdrawsSignatureChecker : public BaseSignatureChecker
{
protected:
    const CTransaction* txTo;
    const CTxOutValue nInValue;
    const unsigned int nIn;
    virtual bool VerifySignature(const std::vector<unsigned char>& vchSig, const CPubKey& vchPubKey, const uint256& sighash) const;

public:
    TransactionNoWithdrawsSignatureChecker(const CTransaction* txToIn, unsigned int nInIn, const CTxOutValue& nInValueIn) : txTo(txToIn), nInValue(nInValueIn), nIn(nInIn) {}
    bool CheckSig(const std::vector<unsigned char>& scriptSig, const std::vector<unsigned char>& vchPubKey, const CScript& scriptCode) const;
    bool CheckLockTime(const CScriptNum& nLockTime, bool fSequence = false) const;
    CTxOutValue GetValueIn() const;
};

class MutableTransactionNoWithdrawsSignatureChecker : public TransactionNoWithdrawsSignatureChecker
{
private:
    const CTransaction txTo;

public:
    MutableTransactionNoWithdrawsSignatureChecker(const CMutableTransaction* txToIn, unsigned int nInIn, const CTxOutValue& nInValueIn) : TransactionNoWithdrawsSignatureChecker(&txTo, nInIn, nInValueIn), txTo(*txToIn) {}
};

class TransactionSignatureChecker : public TransactionNoWithdrawsSignatureChecker
{
private:
    const CTxOutValue nInMinusOneValue;
    const CAmount nTransactionFee;
    const int nSpendHeight;

public:
    TransactionSignatureChecker(const CTransaction* txToIn, unsigned int nInIn, CTxOutValue nInValueIn, CTxOutValue nInMinusOneValueIn, CAmount nTransactionFeeIn, int nSpendHeightIn) : TransactionNoWithdrawsSignatureChecker(txToIn, nInIn, nInValueIn), nInMinusOneValue(nInMinusOneValueIn), nTransactionFee(nTransactionFeeIn), nSpendHeight(nSpendHeightIn) {}
    CTxOut GetOutputOffsetFromCurrent(const int offset) const;
    COutPoint GetPrevOut() const;
    CTxOutValue GetValueInPrevIn() const;
    CAmount GetTransactionFee() const;
#ifdef FEDERATED_PEG_SIDECHAIN_ONLY
    bool IsConfirmedBitcoinBlock(const uint256& hash, bool fConservativeConfirmationRequirements) const;
#endif
};

bool EvalScript(std::vector<std::vector<unsigned char> >& stack, const CScript& script, unsigned int flags, const BaseSignatureChecker& checker, ScriptError* error = NULL);
bool VerifyScript(const CScript& scriptSig, const CScript& scriptPubKey, unsigned int flags, const BaseSignatureChecker& checker, ScriptError* error = NULL);

#endif // BITCOIN_SCRIPT_INTERPRETER_H
