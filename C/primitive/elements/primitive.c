/* This module implements the 'primitive.h' interface for the Elements application of Simplicity.
 */
#include "primitive.h"

#include "jets.h"
#include "../../callonce.h"
#include "../../prefix.h"
#include "../../tag.h"
#include "../../unreachable.h"

#define PRIMITIVE_TAG(s) SIMPLICITY_PREFIX "\x1F" "Primitive\x1F" "Elements\x1F" s
#define JET_TAG SIMPLICITY_PREFIX "\x1F" "Jet"

/* An enumeration of all the types we need to construct to specify the input and output types of all jets created by 'decodeJet'. */
enum TypeNamesForJets {
#include "primitiveEnumTy.inc"
  NumberOfTypeNames
};

/* Allocate a fresh set of unification variables bound to at least all the types necessary
 * for all the jets that can be created by 'decodeJet', and also the type 'TWO^256',
 * and also allocate space for 'extra_var_len' many unification variables.
 * Return the number of non-trivial bindings created.
 *
 * However, if malloc fails, then return 0.
 *
 * Precondition: NULL != bound_var;
 *               NULL != word256_ix;
 *               NULL != extra_var_start;
 *
 * Postcondition: Either '*bound_var == NULL' and the function returns 0
 *                or 'unification_var (*bound_var)[*extra_var_start + extra_var_len]' is an array of unification variables
 *                   such that for any 'jet : A |- B' there is some 'i < *extra_var_start' and 'j < *extra_var_start' such that
 *                      '(*bound_var)[i]' is bound to 'A' and '(*bound_var)[j]' is bound to 'B'
 *                   and, '*word256_ix < *extra_var_start' and '(*bound_var)[*word256_ix]' is bound the type 'TWO^256'
 */
size_t mallocBoundVars(unification_var** bound_var, size_t* word256_ix, size_t* extra_var_start, size_t extra_var_len) {
  _Static_assert(NumberOfTypeNames <= SIZE_MAX / sizeof(unification_var), "NumberOfTypeNames is too large");
  *bound_var = extra_var_len <= SIZE_MAX / sizeof(unification_var) - NumberOfTypeNames
             ? malloc((NumberOfTypeNames + extra_var_len) * sizeof(unification_var))
             : NULL;
  if (!(*bound_var)) return 0;
#include "primitiveInitTy.inc"
  *word256_ix = ty_w256;
  *extra_var_start = NumberOfTypeNames;

  /* 'ty_u' is a trivial binding, so we made 'NumberOfTypeNames - 1' non-trivial bindings. */
  return NumberOfTypeNames - 1;
};

/* An enumeration of the names of Elements specific jets and primitives. */
typedef enum jetName
{
#include "primitiveEnumJet.inc"
  NUMBER_OF_JET_NAMES
} jetName;

static int32_t either(jetName* result, jetName a, jetName b, bitstream* stream) {
  int32_t bit = getBit(stream);
  if (bit < 0) return bit;
  *result = bit ? b : a;
  return 0;
}

/* Decode an Elements specific jet name from 'stream' into 'result'.
 * All jets begin with a bit prefix of '1' which needs to have already been consumed from the 'stream'.
 * Returns 'SIMPLICITY_ERR_DATA_OUT_OF_RANGE' if the stream's prefix doesn't match any valid code for a jet.
 * Returns 'SIMPLICITY_ERR_BITSTRING_EOF' if not enough bits are available in the 'stream'.
 * Returns 'SIMPLICITY_ERR_BITSTREAM_ERROR' if an I/O error occurs when reading from the 'stream'.
 * In the above error cases, 'result' may be modified.
 * Returns 0 if successful.
 *
 * Precondition: NULL != result
 *               NULL != stream
 */
static int32_t decodePrimitive(jetName* result, bitstream* stream) {
  int32_t bit = getBit(stream);
  if (bit < 0) return bit;
  if (!bit) {
    int32_t code = getNBits(5, stream);
    if (code < 0) return code;

    switch (code) {
     case 0x0: return either(result, VERSION, LOCK_TIME, stream);
     case 0x1: *result = INPUT_IS_PEGIN; return 0;
     case 0x2: *result = INPUT_PREV_OUTPOINT; return 0;
     case 0x3: *result = INPUT_ASSET; return 0;
     case 0x4: return either(result, INPUT_AMOUNT, INPUT_SCRIPT_HASH, stream);
     case 0x5: *result = INPUT_SEQUENCE; return 0;
     case 0x6: *result = INPUT_ISSUANCE_BLINDING; return 0;
     case 0x7: *result = INPUT_ISSUANCE_CONTRACT; return 0;
     case 0x8: return either(result, INPUT_ISSUANCE_ENTROPY, INPUT_ISSUANCE_ASSET_AMT, stream);
     case 0x9: *result = INPUT_ISSUANCE_TOKEN_AMT; return 0;
     case 0xa: *result = OUTPUT_ASSET; return 0;
     case 0xb: *result = OUTPUT_AMOUNT; return 0;
     case 0xc: return either(result, OUTPUT_NONCE, OUTPUT_SCRIPT_HASH, stream);
     case 0xd: *result = OUTPUT_NULL_DATUM; return 0;
     case 0xe: *result = SCRIPT_CMR; return 0;
     case 0xf: *result = CURRENT_INDEX; return 0;
     case 0x10: *result = CURRENT_IS_PEGIN; return 0;
     case 0x11: *result = CURRENT_PREV_OUTPOINT; return 0;
     case 0x12: *result = CURRENT_ASSET; return 0;
     case 0x13: *result = CURRENT_AMOUNT; return 0;
     case 0x14: *result = CURRENT_SCRIPT_HASH; return 0;
     case 0x15: *result = CURRENT_SEQUENCE; return 0;
     case 0x16: *result = CURRENT_ISSUANCE_BLINDING; return 0;
     case 0x17: *result = CURRENT_ISSUANCE_CONTRACT; return 0;
     case 0x18: *result = CURRENT_ISSUANCE_ENTROPY; return 0;
     case 0x19: *result = CURRENT_ISSUANCE_ASSET_AMT; return 0;
     case 0x1a: *result = CURRENT_ISSUANCE_TOKEN_AMT; return 0;
     case 0x1b: *result = INPUTS_HASH; return 0;
     case 0x1c: *result = OUTPUTS_HASH; return 0;
     case 0x1d: *result = NUM_INPUTS; return 0;
     case 0x1e: *result = NUM_OUTPUTS; return 0;
     case 0x1f:
      /* FEE is not yet implemented.  Disable it. */
      *result = FEE; return SIMPLICITY_ERR_DATA_OUT_OF_RANGE;
    }
    assert(false);
    UNREACHABLE;
  } else {
    bit = getBit(stream);
    if (bit < 0) return bit;
    if (!bit) {
      /* Core jets */
      int32_t code = decodeUptoMaxInt(stream);
      if (code < 0) return code;

      switch (code) {
       case 2: /* Arith jets chapter */
        code = decodeUptoMaxInt(stream);
        if (code < 0) return code;

        switch (code) {
         case 2: /* FullAdd */
          code = decodeUptoMaxInt(stream);
          if (code < 0) return code;
          switch (code) {
           case 5: *result = FULL_ADD_32; return 0;
          }
          break;
         case 3: /* Add */
          code = decodeUptoMaxInt(stream);
          if (code < 0) return code;
          switch (code) {
           case 5: *result = ADD_32; return 0;
          }
          break;
         case 7: /* FullSubtract */
          code = decodeUptoMaxInt(stream);
          if (code < 0) return code;
          switch (code) {
           case 5: *result = FULL_SUBTRACT_32; return 0;
          }
          break;
         case 8: /* Subtract */
          code = decodeUptoMaxInt(stream);
          if (code < 0) return code;
          switch (code) {
           case 5: *result = SUBTRACT_32; return 0;
          }
          break;
         case 12: /* FullMultiply */
          code = decodeUptoMaxInt(stream);
          if (code < 0) return code;
          switch (code) {
           case 5: *result = FULL_MULTIPLY_32; return 0;
          }
          break;
         case 13: /* Multiply */
          code = decodeUptoMaxInt(stream);
          if (code < 0) return code;
          switch (code) {
           case 5: *result = MULTIPLY_32; return 0;
          }
          break;
        }
        break;
       case 3: /* Hash jets chapter */
        code = decodeUptoMaxInt(stream);
        if (code < 0) return code;
        switch (code) {
         case 1: /* SHA-256 section */
          code = decodeUptoMaxInt(stream);
          if (code < 0) return code;
          switch (code) {
           case 1: *result = SHA_256_BLOCK; return 0;
          }
          break;
        }
        break;
      }
      return SIMPLICITY_ERR_DATA_OUT_OF_RANGE;

    } else {
      /* Elements specific jets go here */
      return SIMPLICITY_ERR_DATA_OUT_OF_RANGE;
    }
  }
}

/* Cached copy of each node for all the Elements specific jets.
 * Only to be accessed through 'jetNode'.
 */
static once_flag static_initialized = ONCE_FLAG_INIT;
static dag_node jet_node[] = {
#include "primitiveJetNode.inc"
 };
static void static_initialize(void) {
  {
    sha256_midstate jet_iv;
    MK_TAG(&jet_iv, JET_TAG);

#define MK_JET(name, h0, h1, h2, h3, h4, h5, h6, h7) \
  do { \
    jet_node[name].cmr = jet_iv; \
    sha256_compression(jet_node[name].cmr.s, (uint32_t[16]){ [8] = h0, h1, h2, h3, h4, h5, h6, h7 }); \
  } while(0)

    /* Jets are identified by their specification's identity Merkle roots. */
#include "primitiveInitJet.inc"
#undef MK_JET

  }
#include "primitiveInitPrim.inc"
}

/* Return a copy of the Simplicity node corresponding to the given Elements specific jet 'name'.
 */
static dag_node jetNode(jetName name) {
  call_once(&static_initialized, &static_initialize);

  return jet_node[name];
}

/* Decode an Elements specific jet from 'stream' into 'node'.
 * All jets begin with a bit prefix of '1' which needs to have already been consumed from the 'stream'.
 * Returns 'SIMPLICITY_ERR_DATA_OUT_OF_RANGE' if the stream's prefix doesn't match any valid code for a jet.
 * Returns 'SIMPLICITY_ERR_BITSTRING_EOF' if not enough bits are available in the 'stream'.
 * Returns 'SIMPLICITY_ERR_BITSTREAM_ERROR' if an I/O error occurs when reading from the 'stream'.
 * In the above error cases, 'dag' may be modified.
 * Returns 0 if successful.
 *
 * Precondition: NULL != node
 *               NULL != stream
 */
int32_t decodeJet(dag_node* node, bitstream* stream) {
  jetName name;
  int32_t err = decodePrimitive(&name, stream);
  if (err < 0) return err;
  *node = jetNode(name);
  return 0;
}
