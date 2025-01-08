// MIT License
//
// Copyright (c) 2024 Hewlett Packard Enterprise Development LP
//		
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.


#include <sst_config.h>
/*
  Install the python library
 */
#include <sst/core/model/element_python.h>

namespace SST {
namespace CAMSHAP {

char pycamshap[] = {
#include "pycamshap.inc"
    0x00};

class CAMSHAP_PyModule : public SSTElementPythonModule {
public:
    CAMSHAP_PyModule(std::string library) :
        SSTElementPythonModule(library)
    {
        auto primary_module = createPrimaryModule(pycamshap,"pycamshap.py");
    }

    SST_ELI_REGISTER_PYTHON_MODULE(
        SST::CAMSHAP::CAMSHAP_PyModule,
        "camshap",
        SST_ELI_ELEMENT_VERSION(1,0,0)
    )

    SST_ELI_EXPORT(SST::CAMSHAP::CAMSHAP_PyModule)    
};

}
}