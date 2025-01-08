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

import numpy as np

def configureNode(hwConfig, swConfig, partConfig):
    """! 
    @brief      Configure NoC.

    @param[in]  hwConfig:               Hardware configuration
    @param[in]  swConfig:               Software configuration
    @param[in]  partConfig:             Partition configuration
    
    @return     nocConfig
    """

    nocConfig = {}
    nocConfig['node'] = hwConfig['node']
    nocConfig['node']['linkLatency'] = f"{int(np.ceil(swConfig['numFeature']/4))}ns"

    nocConfig['node']['control_node'] = hwConfig['control_node']
    nocConfig['node']['control_node']['param']['verbose']  = swConfig['verbose']
    nocConfig['node']['control_node']['param']['mask']  = swConfig['mask']
    nocConfig['node']['control_node']['param']['outputDir']  = swConfig['logDir']
    nocConfig['node']['control_node']['param']['freq']  = hwConfig['default']['freq']
    nocConfig['node']['control_node']['param']['numClass']  = swConfig['numClass']
    nocConfig['node']['control_node']['param']['numFeature']  = swConfig['numFeature']
    nocConfig['node']['control_node']['param']['instructionTable']  = partConfig['node']['instruction']
    nocConfig['node']['control_node']['param']['dataMemoryTable']  = partConfig['node']['data']
    nocConfig['node']['control_node']['linkLatency']  = f"{int(np.ceil(swConfig['numFeature']/4))}ns"

    nocConfig['node']['router'] = hwConfig['router']
    nocConfig['node']['router']['param']['verbose']  = swConfig['verbose']
    nocConfig['node']['router']['param']['mask']  = swConfig['mask']
    nocConfig['node']['router']['param']['outputDir']  = swConfig['logDir']
    nocConfig['node']['router']['param']['freq']  = hwConfig['default']['freq']
    nocConfig['node']['router']['param']['numPort'] = hwConfig['node']['numPort']
    nocConfig['node']['router']['param']['numLevel'] = hwConfig['node']['numLevel']
    nocConfig['node']['router']['linkLatency']  = f"{int(np.ceil(swConfig['numFeature']/4))}ns"

    nocConfig['node']['tile'] = {}
    nocConfig['node']['tile']['linkLatency']  = f"{int(np.ceil(swConfig['numFeature']/4))}ns"
    nocConfig['node']['tile']['numCore'] = hwConfig['node']['numCore']
    nocConfig['node']['tile']['tilePerClass'] = partConfig['tile']['tilePerClass']

    nocConfig['node']['tile']['control_tile'] = hwConfig['control_tile']
    nocConfig['node']['tile']['control_tile']['param']['verbose']  = swConfig['verbose']
    nocConfig['node']['tile']['control_tile']['param']['mask']  = swConfig['mask']
    nocConfig['node']['tile']['control_tile']['param']['outputDir']  = swConfig['logDir']
    nocConfig['node']['tile']['control_tile']['param']['freq']  = hwConfig['default']['freq']
    nocConfig['node']['tile']['control_tile']['param']['numCore'] = hwConfig['node']['numCore']
    nocConfig['node']['tile']['control_tile']['param']['dataMemoryTable'] = partConfig['tile']['data']
    nocConfig['node']['tile']['control_tile']['param']['instructionTable'] = partConfig['tile']['instruction']

    nocConfig['node']['tile']['core'] = {}
    nocConfig['node']['tile']['core']['linkLatency']  = hwConfig['default']['linkLatency']

    nocConfig['node']['tile']['core']['control_core'] = hwConfig['control_core']
    nocConfig['node']['tile']['core']['control_core']['param']['verbose']  = swConfig['verbose']
    nocConfig['node']['tile']['core']['control_core']['param']['mask']  = swConfig['mask']
    nocConfig['node']['tile']['core']['control_core']['param']['outputDir']  = swConfig['logDir']
    nocConfig['node']['tile']['core']['control_core']['param']['freq']  = hwConfig['default']['freq']
    nocConfig['node']['tile']['core']['control_core']['param']['numCol'] = hwConfig['node']['numCol']
    nocConfig['node']['tile']['core']['control_core']['param']['numRow'] = hwConfig['node']['numRow']
    nocConfig['node']['tile']['core']['control_core']['param']['instructionTable'] = partConfig['core']['instruction']
    nocConfig['node']['tile']['core']['control_core']['dataMemoryTable'] = partConfig['core']['data']

    nocConfig['node']['tile']['core']['acam'] = hwConfig['acam']
    nocConfig['node']['tile']['core']['acam']['param']['verbose']  = swConfig['verbose']
    nocConfig['node']['tile']['core']['acam']['param']['mask']  = swConfig['mask']
    nocConfig['node']['tile']['core']['acam']['param']['outputDir']  = swConfig['logDir']
    nocConfig['node']['tile']['core']['acam']['param']['freq']  = hwConfig['default']['freq']
    nocConfig['node']['tile']['core']['acam']['param']['numCol'] = hwConfig['node']['numCol']
    nocConfig['node']['tile']['core']['acam']['param']['numRow'] = hwConfig['node']['numRow']
    nocConfig['node']['tile']['core']['acam']['param']['gList'] = partConfig['acam']['gList'].tolist()
    nocConfig['node']['tile']['core']['acam']['acamThLow'] = partConfig['acam']['acamThLow']
    nocConfig['node']['tile']['core']['acam']['acamThHigh'] = partConfig['acam']['acamThHigh']
    nocConfig['node']['tile']['core']['acam']['acamThXLow'] = partConfig['acam']['acamThXLow']
    nocConfig['node']['tile']['core']['acam']['acamThXHigh'] = partConfig['acam']['acamThXHigh']

    nocConfig['node']['tile']['core']['mpe'] = hwConfig['mpe']
    nocConfig['node']['tile']['core']['mpe']['param']['verbose']  = swConfig['verbose']
    nocConfig['node']['tile']['core']['mpe']['param']['mask']  = swConfig['mask']
    nocConfig['node']['tile']['core']['mpe']['param']['outputDir']  = swConfig['logDir']
    nocConfig['node']['tile']['core']['mpe']['param']['freq']  = hwConfig['default']['freq']
    nocConfig['node']['tile']['core']['mpe']['param']['numRow'] = hwConfig['node']['numRow']

    nocConfig['node']['tile']['core']['mmr'] = hwConfig['mmr']
    nocConfig['node']['tile']['core']['mmr']['param']['verbose']  = swConfig['verbose']
    nocConfig['node']['tile']['core']['mmr']['param']['mask']  = swConfig['mask']
    nocConfig['node']['tile']['core']['mmr']['param']['outputDir']  = swConfig['logDir']
    nocConfig['node']['tile']['core']['mmr']['param']['freq']  = hwConfig['default']['freq']

    return nocConfig