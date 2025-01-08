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

import sst
from sst.camshap import Params
import numpy as np
import copy

params = Params()

class nocBase():
    def __init__(self, param):
        self.params = copy.deepcopy(param)
    def buildLink(self, source, sourceIndex, destination, destinationIndex, linkLatency, noCut=False):
        linkList = []
        for s in sourceIndex:
            for d in destinationIndex:
                link = sst.Link("{}{:d}_{}{:d}".format(source, s, destination, d), linkLatency)
                if noCut:
                    link.setNoCut()
                linkList.append(link)
        return linkList

class node(nocBase):
    """!
    @brief      Node class with Tree NoC
    """
    def __init__(self, param):
        """!
        @brief      Constructor
        @details    
        @param[in]  param:          Configuration parameters (from configure.py) 
        """
        self.params = copy.deepcopy(param)

        self.control_node   = control_node(self.params['control_node'])
        self.router         = router(self.params['router'])
        self.tile           = tile(self.params['tile'])

    def build(self):
        """!
        @brief      Build node
        @details    Define 'linkList', which is the list of links connecting each sub-class. Pass the list of input links and output links for each sub-class. 
        @param      linkList:       List of links
        """
        numPort = self.params['numPort']
        numLevel = self.params['numLevel']
        numTotalTile = pow(numPort, numLevel)

        linkList = {
            'Node-Router'           : self.buildLink('Node', [0], 'Router', [0], self.params['linkLatency']),
            'Router-Tile'           : self.buildLink('Router', [0], 'Tile', [x for x in range(numTotalTile)], self.params['linkLatency']),
            'Tile-Router'           : self.buildLink('Tile', [x for x in range(numTotalTile)], 'Router', [0], self.params['linkLatency']),
            'Router-Node'           : self.buildLink('Router', [0], 'Node', [0], self.params['linkLatency']),
        }

        self.control_node.build(linkList['Node-Router'], linkList['Router-Node'])
        self.router.build(linkList['Node-Router'], linkList['Router-Tile'], linkList['Tile-Router'], linkList['Router-Node'])
        self.tile.build(linkList['Router-Tile'], linkList['Tile-Router'])

class control_node(nocBase):
    """!
    @brief      Control in Node
    """
    def build(self, toRouterLink, fromRouterLink):
        """!
        @brief      Build 'control_node'
        @details    Build sst component (camshap.control_node).
        @param[in]  toRouterLink:       List of output links.
        @param[in]  fromRouterLink:     List of input links.
        """
        name = 'Node_ctrl'
        component = sst.Component(name, 'camshap.control_node')
        component.addParam('id', 0)
        component.addParam('name', name)
        component.addParams(self.params['param'])
        component.addLink(toRouterLink[0],      "toRouterPort")
        component.addLink(fromRouterLink[0],    "fromRouterPort")

class router(nocBase):
    """!
    @brief      Router
    """
    def build(self, fromUpLink, toDownLink, fromDownLink, toUpLink):
        """!
        @brief      Build 'router'
        @details    Build sst component (camshap.router).
        @param[in]  fromUpLink:         List of input link from control_node.
        @param[in]  toDownLink:         List of output links to control_tile.
        @param[in]  fromDownLink:       List of input links from control_tile.
        @param[in]  toUpLink:           List of output link to control_node.
        """
        def buildComponent(self, id, fromUpLink, toDownLink, fromDownLink, toUpLink):
            name = 'Router{:d}'.format(id)
            component = sst.Component(name, 'camshap.router')
            component.addParam('id', id)
            component.addParam('name', name)
            component.addParams(self.params['param'])
            component.addLink(fromUpLink[0], 'fromUpPort')
            for j, link in enumerate(toDownLink):
                component.addLink(link, 'toDownPort{:d}'.format(j))
            for j, link in enumerate(fromDownLink):
                component.addLink(link, 'fromDownPort{:d}'.format(j))
            component.addLink(toUpLink[0], 'toUpPort')
            
        numPort = self.params['param']['numPort']
        numLevel = self.params['param']['numLevel']
        id = 0

        if (numLevel > 1):
            # Last level (numLevel-1) - 'Router-Core'
            linkListIn = []
            linkListOut = []
            for i in range(pow(numPort, numLevel-1)):
                linkIn = sst.Link('Router{:d}In'.format(id), self.params['linkLatency'])
                linkListIn.append(linkIn)
                linkOut = sst.Link('Router{:d}Out'.format(id), self.params['linkLatency'])
                linkListOut.append(linkOut)
                buildComponent(self, id, [linkIn], toDownLink[i*numPort:(i+1)*numPort], fromDownLink[i*numPort:(i+1)*numPort], [linkOut])
                id += 1

            # Intermediate level (1 ~ numLevel-2) - 'Router-Router' 
            for l in range(numLevel-2, 0, -1):
                linkNextTo = linkListIn
                linkNextFrom = linkListOut
                linkListIn = []
                linkListOut = []
                for i in range(pow(numPort, l)):
                    linkIn = sst.Link('Router{:d}In'.format(id), self.params['linkLatency'])
                    linkListIn.append(linkIn)
                    linkOut = sst.Link('Router{:d}Out'.format(id), self.params['linkLatency'])
                    linkListOut.append(linkOut)
                    buildComponent(self, id, [linkIn], linkNextTo[i*numPort:(i+1)*numPort], linkNextFrom[i*numPort:(i+1)*numPort], [linkOut])
                    id += 1
            
            # First level (0) - 'Control-Router' 
            linkNextTo = linkListIn
            linkNextFrom = linkListOut
            buildComponent(self, id, fromUpLink, linkNextTo[:numPort], linkNextFrom[:numPort], toUpLink)
            id += 1
        
        # When it is a single level, demux components take both inputLink and outputLink.
        else:
            buildComponent(self, id, fromUpLink, toDownLink, fromDownLink, toUpLink)

class tile(nocBase):
    """!
    @brief      Tile class
    """
    def __init__(self, param):
        """!
        @brief      Tile constructor
        @details    
        @param[in]  param:          Configuration parameters 
        """
        self.params = copy.deepcopy(param)

        self.control_tile = control_tile(self.params['control_tile']['param'])
        self.core = core(self.params['core'])
    def build(self, fromUpLink, toUpLink):
        """!
        @brief      Build tile
        @details    Build tiles with control_tile and core
        @param[in]  fromUpLink:         List of input links from router.
        @param[in]  toUpLink:           List of output links to router.
        """

        numTile = len(fromUpLink)
        numCore = self.params['numCore']
        tilePerClass = self.params['tilePerClass']

        for t in range(numTile):
            linkList = {
                'Tile-Core'               : self.buildLink('Tile', [t], 'Core', [x for x in range(numCore)], self.params['linkLatency']),
                'Core-Tile'             : self.buildLink('Core', [x for x in range(numCore)], 'Tile', [t], self.params['linkLatency']),
            }
            indexClass = np.argwhere((tilePerClass[:, 0] <= t) & (t <= tilePerClass[:, 1]))
            if len(indexClass) == 0:
                indexClass = 0
            else:
                indexClass = indexClass[0][0] + 1
            self.control_tile.build(t, indexClass, fromUpLink[t], toUpLink[t], linkList['Tile-Core'], linkList['Core-Tile'])
            self.core.build(t, linkList['Tile-Core'], linkList['Core-Tile'])

class control_tile(nocBase):
    """!
    @brief      Control in Tile
    """
    def build(self, t, indexClass, fromUpLink, toUpLink, toCoreLink, fromCoreLink):
        """!
        @brief      Build 'control_tile'
        @details    Build sst component (camshap.control_tile).
        @param[in]  fromUpLink:         List of input links from router.
        @param[in]  toUpLink:           List of output links to router.
        @param[in]  toCoreLink:         List of output links to core.
        @param[in]  fromCoreLink:       List of input links from core.
        """
        name = 'T{:02d}_ctrl'.format(t)
        component = sst.Component(name, 'camshap.control_tile')
        component.addParam('id', t)
        component.addParam('name', name)
        component.addParam('indexClass', indexClass)
        component.addParams(self.params)
        component.addLink(fromUpLink, "fromRouterPort")
        component.addLink(toUpLink, "toRouterPort")
        for j, link in enumerate(toCoreLink):
            component.addLink(link, 'toCorePort{:d}'.format(j))
        for j, link in enumerate(fromCoreLink):
            component.addLink(link, 'fromCorePort{:d}'.format(j))

class core(nocBase):
    """!
    @brief      Core class
    """
    def __init__(self, param):
        """!
        @brief      Core constructor
        @details    
        @param[in]  param:          Configuration parameters 
        """
        self.params = copy.deepcopy(param)

        self.control_core = control_core(self.params['control_core']['param'])
        self.acam = acam(self.params['acam']['param'])
        self.mpe = mpe(self.params['mpe']['param'])
        self.mmr = mmr(self.params['mmr']['param'])

    def build(self, t, fromUpLink, toUpLink):
        """!
        @brief      Build core
        @details    Build cores with control_core, acam, mpe, mmr.
        @param[in]  fromUpLink:         List of input links from tile.
        @param[in]  toUpLink:           List of output links to tile.
        """

        numCore = len(fromUpLink)

        for c in range(numCore):
            linkList = {
                'Core-CAM'              : self.buildLink('Tile{:d}Core'.format(t), [c], 'CAM', [c], self.params['linkLatency']),
                'Core-CAMD'             : self.buildLink('Tile{:d}Core'.format(t), [c], 'CAMD', [c], self.params['linkLatency']),
                'CAM-Core'              : self.buildLink('Tile{:d}CAM'.format(t), [c], 'Core', [c], self.params['linkLatency']),
                'CAM-MPE'               : self.buildLink('Tile{:d}CAM'.format(t), [c], 'MPE', [c], self.params['linkLatency']),
                
                'Core-MPE'              : self.buildLink('Tile{:d}Core'.format(t), [c], 'MPE', [c], self.params['linkLatency']),
                'MPE-Core'              : self.buildLink('Tile{:d}MPE'.format(t), [c], 'Core', [c], self.params['linkLatency']),
                'MPE-MMR'               : self.buildLink('Tile{:d}MPE'.format(t), [c], 'MMR', [c], self.params['linkLatency']),

                'Core-MMR'              : self.buildLink('Tile{:d}Core'.format(t), [c], 'MMR', [c], self.params['linkLatency']),
                'MMR-Core'              : self.buildLink('Tile{:d}MMR'.format(t), [c], 'Core', [c], self.params['linkLatency'])
            }
            self.control_core.build(numCore, t, c, self.params['control_core']['dataMemoryTable'][t][c], fromUpLink[c], toUpLink[c], linkList['Core-CAM'], linkList['Core-CAMD'], linkList['CAM-Core'], linkList['Core-MPE'], linkList['MPE-Core'], linkList['Core-MMR'], linkList['MMR-Core'])
            self.acam.build(numCore, t, c, self.params['acam']['acamThLow'][t, c, :, :], self.params['acam']['acamThHigh'][t, c, :, :], self.params['acam']['acamThXLow'][t, c, :, :], self.params['acam']['acamThXHigh'][t, c, :, :], linkList['Core-CAM'], linkList['Core-CAMD'], linkList['CAM-Core'], linkList['CAM-MPE'])
            self.mpe.build(numCore, t, c, linkList['Core-MPE'], linkList['CAM-MPE'], linkList['MPE-Core'], linkList['MPE-MMR'])
            self.mmr.build(numCore, t, c, linkList['Core-MMR'], linkList['MPE-MMR'], linkList['MMR-Core'])

class control_core(nocBase):
    """!
    @brief      Control in Core
    """
    def build(self, numCore, t, c, data,  fromTileLink, toTileLink, toCAMLink, toCAMDLink, fromCAMLink, toMPELink, fromMPELink, toMMRLink, fromMMRLink):
        """!
        @brief      Build 'control_core'
        @details    Build sst component (camshap.control_core).
        @param[in]  numCore:                        Number of cores in a tile.
        @param[in]  t, c:                           Tile and core index.
        @param[in]  data:                           Data memory table.
        @param[in]  fromTileLink, toTileLink:       List of input and output links from tile.
        @param[in]  toCAMLink, toCAMDLink:          List of output links to CAM and CAMD.
        @param[in]  fromCAMLink:                    List of input links from CAM.
        @param[in]  toMPELink, fromMPELink:         List of output and input links to MPE.
        @param[in]  toMMRLink, fromMMRLink:         List of output and input links to MMR.
        """
        name = 'T{:02d}C{:02d}_ctrl'.format(t, c)
        component = sst.Component(name, 'camshap.control_core')
        component.addParam('id', t*numCore+c)
        component.addParam('name', name)
        component.addParams(self.params)
        component.addParam('dataMemoryTable', data)
        component.addLink(toTileLink,       "toTilePort")
        component.addLink(fromTileLink,     "fromTilePort")
        component.addLink(toCAMLink[0],     "toCAMPort")
        component.addLink(toCAMDLink[0],    "toCAMDataPort")
        component.addLink(toMPELink[0],     "toMPEPort")
        component.addLink(fromMPELink[0],   "fromMPEPort")
        component.addLink(toMMRLink[0],     "toMMRPort")
        component.addLink(fromMMRLink[0],   "fromMMRPort")

class acam(nocBase):
    """!
    @brief      ACAM class
    """
    def build(self, numCore, t, c, acamThLow, acamThHigh, acamThXLow, acamThXHigh, reqeustLink, dataLink, responseLink, outputLink):
        """!
        @brief      Build 'acam'
        @details    Build sst component (camshap.acam).
        @param[in]  numCore:                        Number of cores in a tile.
        @param[in]  t, c:                           Tile and core index.
        @param[in]  acamThLow, acamThHigh:          ACAM threshold low and high.
        @param[in]  acamThXLow, acamThXHigh:        ACAM threshold X low and high.
        @param[in]  reqeustLink, dataLink, responseLink, outputLink: List of input and output links.
        """
        name = 'T{:02d}C{:02d}_acam'.format(t, c)
        component = sst.Component(name, 'camshap.acam')
        component.addParam('id', (t*numCore+c))
        component.addParam('name', name)
        component.addParams(self.params)
        component.addParam('acamThLow', acamThLow.flatten().tolist())
        component.addParam('acamThHigh', acamThHigh.flatten().tolist())
        component.addParam('acamThXLow', acamThXLow.flatten().tolist())
        component.addParam('acamThXHigh', acamThXHigh.flatten().tolist())
        component.addLink(outputLink[0],    "outputPort")
        component.addLink(reqeustLink[0],   "requestPort")
        component.addLink(dataLink[0],      "dataPort")

class mpe(nocBase):
    """!
    @brief      MPE class
    """
    def build(self, numCore, t, c, reqeustLink, dataLink, responseLink, outputLink):
        """!
        @brief      Build 'mpe'
        @details    Build sst component (camshap.mpe).
        @param[in]  numCore:                        Number of cores in a tile.
        @param[in]  t, c:                           Tile and core index.
        @param[in]  reqeustLink, dataLink, responseLink, outputLink: List of input and output links.
        """
        name = 'T{:02d}C{:02d}__mpe'.format(t, c)
        component = sst.Component(name, 'camshap.mpe')
        component.addParam('id', (t*numCore+ c))
        component.addParam('name', name)
        component.addParams(self.params)
        component.addLink(responseLink[0],  "responsePort")
        component.addLink(outputLink[0],    "outputPort")
        component.addLink(reqeustLink[0],   "requestPort")
        component.addLink(dataLink[0],      "dataPort")
        
class mmr(nocBase):
    """!
    @brief      MMR class
    """
    def build(self, numCore, t, c, reqeustLink, dataLink, responseLink):
        """!
        @brief      Build 'mmr'
        @details    Build sst component (camshap.mmr).
        @param[in]  numCore:                        Number of cores in a tile.
        @param[in]  t, c:                           Tile and core index.
        @param[in]  reqeustLink, dataLink, responseLink: List of input links.
        """
        name = 'T{:02d}C{:02d}__mmr'.format(t, c)
        component = sst.Component(name, 'camshap.mmr')
        component.addParam('id', (t*numCore+ c))
        component.addParam('name', name)
        component.addParams(self.params)
        component.addLink(responseLink[0],  "responsePort")
        component.addLink(reqeustLink[0],   "requestPort")
        component.addLink(dataLink[0],      "dataPort")
               
def setStatistic(level, outputDir, compList):
    sst.setStatisticLoadLevel(level)
    sst.setStatisticOutput("sst.statOutputCSV", {"filepath" : outputDir+"Output.csv", "separator" : ", " })
    if (len(compList) == 0):
        sst.enableAllStatisticsForAllComponents()