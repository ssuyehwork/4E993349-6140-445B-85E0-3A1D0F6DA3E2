#include "OCRManager.h"

OCRManager& OCRManager::instance() {
    static OCRManager inst;
    return inst;
}