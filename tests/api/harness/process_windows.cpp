#include <directmetal/api_harness/harness.hpp>

#include <windows.h>

#include <fcntl.h>
#include <io.h>
#include <stdio.h>

namespace directmetal::api_harness {

void prepare_process() {
    // Text-mode stdout would turn the document's newline into CRLF.
    _setmode(_fileno(stdout), _O_BINARY);
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
}

} // namespace directmetal::api_harness
