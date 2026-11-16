// Compiles DM_STUB with markers.hpp as the only include, so the header stays self-contained.
#include <directmetal/frontends/markers.hpp>

HRESULT markers_header_check() {
    DM_STUB("D3D11CreateDevice");
}
