# MIT License

# Copyright (c) 2024 Hewlett Packard Enterprise Development LP
		
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:

# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.

# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

from util import *
from noc import *
from configure import *

if __name__ == "__main__":

    arg = readArg()

    inputConfig = sstConfig(arg['idx'], arg['folder'])
    print("Model configuration ... \n")
    mConfig = modelConfig(arg, inputConfig.sw.getConfig())
    mConfig_dict = mConfig.getConfig(overwrite=True)

    partConfig = {}
    print("aCAM configuration ... \n")
    aConfig = acamConfig(inputConfig.hw.getConfig(), inputConfig.sw.getConfig(), mConfig_dict)
    partConfig['acam'], tilePerClass = aConfig.getConfig()

    print("Node configuration ... \n")
    nConfig = nodeConfig(inputConfig.sw.getConfig(), mConfig_dict)
    partConfig['node'] = nConfig.getConfig()

    print("Tile configuration ... \n")
    tConfig = tileConfig(inputConfig.hw.getConfig(), inputConfig.sw.getConfig(), mConfig_dict, tilePerClass)
    partConfig['tile'] = tConfig.getConfig()

    print("Core configuration ... \n")
    cConfig = coreConfig(inputConfig.hw.getConfig(), inputConfig.sw.getConfig(), partConfig['acam']['acamValue'])
    partConfig['core'] = cConfig.getConfig()

    print("SST configuration ... \n")
    nocConfig = configureNode(inputConfig.hw.getConfig(), inputConfig.sw.getConfig(), partConfig)
    sstNode = node(nocConfig['node'])
    sstNode.build()
    setStatistic(2, inputConfig.sw.getConfig()["logDir"], [])