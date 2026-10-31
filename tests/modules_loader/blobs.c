/* Embedded test DLL images (see gen/gen.sh). */
#include "ml.h"
#include "modblob.h"
const unsigned char *ml_blob(int which, size_t *n) {
    switch (which) {
    case ML_A:   *n = sizeof blob_k32m_a;   return blob_k32m_a;
    case ML_DEP: *n = sizeof blob_k32m_dep; return blob_k32m_dep;
    case ML_IMP: *n = sizeof blob_k32m_imp; return blob_k32m_imp;
    case ML_FAIL: *n = sizeof blob_k32m_fail; return blob_k32m_fail;
    case ML_BADIMP: *n = sizeof blob_k32m_badimp; return blob_k32m_badimp;
    }
    *n = 0; return NULL;
}
