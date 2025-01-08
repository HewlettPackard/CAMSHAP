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
import argparse
import json
import sys
import os
import struct
import pickle
import math
import copy

class nodeConfig():
    def __init__(self, param, model):
        self.numTest = param['numTest']
        self.numFeature = param['numFeature']
        self.numBase = param['numBase']
        self.numClass = len(param['beginCore'])-1

        self.offsetPartialSHAP = param['offsetPartialSHAP']
        self.offsetSHAP = param['offsetSHAP']
        self.offsetNodeSample = param['offsetNodeSample']
    
        self.model = model

    def getInstruction(self):
        instruction = [ 55,	10,	0,	0,	6,	0,	0,\
                        19,	0,	0,	0,	0,	0,	0,\
                        19,	0,	0,	0,	0,	0,	0,\
                        3,	11,	10,	0,	16,	2,	0,\
                        3,	12,	10,	0,	20,	3,	0,\
                        3,	13,	10,	0,	24,	2,	0,\
                        3,	15,	10,	0,	8,	2,	0,\
                        3,	22,	10,	0,	12,	2,	0,\
                        3,	14,	10,	0,	0,	2,	0,\
                        7,	1,	10,	0,	4,	2,	0,\
                        51,	16,	22,	15,	0,	0,	1,\
                        11,	0,	13,	15,	0,	1,	0,\
                        3,	21,	10,	0,	28,	2,	0,\
                        19,	0,	0,	0,	0,	0,	0,\
                        19,	0,	0,	0,	0,	0,	0,\
                        99,	0,	21,	0,	2053,	0,	0,\
                        19,	23,	0,	0,	0,	0,	0,\
                        35, 0,  10, 0, 28, 2,  0,\
                        7,	2,	11,	0,	0,	2,	0,\
                        7,	3,	12,	0,	0,	2,	0,\
                        19,	19,	19,	0,	1,	0,	0,\
                        19,	23,	23,	0,	1,	0,	0,\
                        83,	4,	2,	1,	0,	0,	12,\
                        19,	11,	11,	0,	4,	0,	0,\
                        19,	0,	0,	0,	0,	0,	0,\
                        83,	3,	3,	4,	0,	0,	0,\
                        19,	0,	0,	0,	0,	0,	0,\
                        19,	0,	0,	0,	0,	0,	0,\
                        39,	0,	12,	3,	0,	2,	0,\
                        19,	12,	12,	0,	4,	0,	0,\
                        19,	0,	0,	0,	0,	0,	0,\
                        99,	0,	23,	22,	2063,	1,	0,\
                        99,	0,	19,	16,	2070,	1,	0,\
                        19, 18, 18, 0,  1,  0,  0,\
                        19, 19, 0,  0,  0,  0,  0,\
                        3,  11, 10, 0,  16, 2,  0,\
                        3,	12,	10,	0,	20,	2,	0,\
                        19,	0,	0,	0,	0,	0,	0,\
                        99, 0,  18, 14, 2076,   1,  0,\
                        19,	20,	16,	0,	2,	1,	0,\
                        51,	13,	13,	15,	0,	0,	0,\
                        19,	17,	17,	0,	1,	0,	0,\
                        19,	19,	0,	0,	0,	0,	0,\
                        19, 18, 0,  0,  0,  0,  0,\
                        11,	0,	12,	20,	0,	0,	0,\
                        39,	0,	12,	0,	0,	2,	0,\
                        19,	0,	0,	0,	0,	0,	0,\
                        99,	0,	17,	14,	2086,	1,	0,\
                        0,	0,	0,	0,	0,	0,	0,\
                        19,	0,	0,	0,	0,	0,	0,\
                        19,	0,	0,	0,	0,	0,	0,\
                        19,	0,	0,	0,	0,	0,	0,\
                        19,	0,	0,	0,	0,	0,	0,\
                        ]
        return instruction

    def getData(self):
        data = np.zeros(32768, dtype=np.uint8)
        data[24576] = self.numTest
        data[24580:24580+4] = list(struct.pack('<f', self.numBase))
        data[24584] = self.numFeature
        data[24588] = self.numClass
        data[24592:24592+4] = list(self.offsetPartialSHAP.to_bytes(4, byteorder='little'))
        data[24596:24596+4] = list(self.offsetSHAP.to_bytes(4, byteorder='little'))
        data[24600:24600+4] = list(self.offsetNodeSample.to_bytes(4, byteorder='little'))
        data[self.offsetNodeSample:self.offsetNodeSample + self.numTest*self.numFeature] = self.model['testSample'].flatten()
        return data.tolist()

    def getConfig(self):
        config  = {}
        config['instruction'] = self.getInstruction()
        config['data'] = self.getData()
        return config

class tileConfig():
    def __init__(self, hwConfig, param, model, tilePerClass):
        self.numTile = pow(hwConfig['node']['numPort'], hwConfig['node']['numLevel'])
        self.tilePerClass = tilePerClass
        self.numFeature = param['numFeature']
        self.numBase = param['numBase']
        
        self.offsetTileSample = param['offsetTileSample']
        self.offsetTileBase = param['offsetTileBase']
        self.offsetTileResult = param['offsetTileResult']

        self.base = model['baseSample']

    def getInstruction(self):
        instruction = [ 55,	10,	0,	0,	4,	0,	0,\
                        19,	0,	0,	0,	0,	0,	0,\
                        19,	0,	0,	0,	0,	0,	0,\
                        19,	10,	10,	0,	128,	0,	0,\
                        19,	16,	0,	0,	4,	0,	0,\
                        19,	0,	0,	0,	0,	0,	0,\
                        3,	11,	10,	0,	8,	2,	0,\
                        3,	12,	10,	0,	12,	2,	0,\
                        3,	13,	10,	0,	20,	2,	0,\
                        3,	14,	10,	0,	0,	2,	0,\
                        3,	15,	10,	0,	4,	2,	0,\
                        3,	21,	10,	0,	24,	2,	0,\
                        19,	0,	0,	0,	0,	0,	0,\
                        19,	0,	0,	0,	0,	0,	0,\
                        99,	0,	21,	0,	2053,	0,	0,\
                        35,	0,	10,	0,	24,	2,	0,\
                        11,	0,	11,	15,	0,	1,	0,\
                        11,	0,	12,	15,	0,	1,	0,\
                        3,	22,	10,	0,	28,	2,	0,\
                        19,	0,	0,	0,	0,	0,	0,\
                        19,	0,	0,	0,	0,	0,	0,\
                        99,	0,	22,	0,	2053,	0,	0,\
                        35,	0,	10,	0,	28,	2,	0,\
                        11,	0,	13,	16,	0,	0,	0,\
                        19,	18,	18,	0,	1,	0,	0,\
                        19,	0,	0,	0,	0,	0,	0,\
                        19,	0,	0,	0,	0,	0,	0,\
                        99,	0,	18,	15,	2059,	1,	0,\
                        51,	12,	12,	15,	0,	0,	0,\
                        19,	17,	17,	0,	1,	0,	0,\
                        19,	18,	0,	0,	0,	0,	0,\
                        19,	0,	0,	0,	0,	0,	0,\
                        99,	0,	17,	14,	2065,	1,	0,\
                        3,	12,	10,	0,	12,	2,	0,\
                        19,	17,	0,	0,	0,	0,	0,\
                        19,	18,	0,	0,	0,	0,	0,\
                        19,	0,	0,	0,	0,	0,	0,\
                        99,	0,	0,	0,	2076,	0,	0,
                        ]
        return instruction

    def getData(self):
        data = np.zeros(17408, dtype=np.uint8)
        data[16512] = self.numBase
        data[16516] = self.numFeature
        data[16520:16520+4] = list(self.offsetTileSample.to_bytes(4, byteorder='little'))
        data[16524:16524+4] = list(self.offsetTileBase.to_bytes(4, byteorder='little'))
        data[16532:16532+4] = list(self.offsetTileResult.to_bytes(4, byteorder='little'))
        data[self.offsetTileBase:self.offsetTileBase + self.numBase*self.numFeature] = self.base.flatten()
        return data.tolist()

    def getConfig(self):
        config  = {}
        config['instruction'] = self.getInstruction()
        config['data'] = self.getData()
        config['tilePerClass'] = self.tilePerClass
        return config

class coreConfig():
    def __init__(self, hwConfig, param, leafValue):
        self.numTile = pow(hwConfig['node']['numPort'], hwConfig['node']['numLevel'])
        self.numCore = hwConfig['node']['numCore']

        self.numBase = param['numBase']
        self.numFeature = param['numFeature']
        
        self.offsetCAM = param['offsetCAM']
        self.offsetCoreSample = param['offsetCoreSample']
        self.offsetCoreBase = param['offsetCoreBase']
        self.offsetValue = param['offsetValue']
        self.offsetWeight = param['offsetWeight']

        self.leafValue = leafValue
        self.wValue = np.array([ 0, 0,  0,  0,  0,  0,  0,  0,  0, 56,  0,  0,  0,  0,  0,  0,  0, 48,\
                            48,  0,  0,  0,  0,  0,  0, 42, 34, 42,  0,  0,  0,  0,  0, 40, 26,\
                            26, 40,  0,  0,  0,  0, 36, 20, 16, 20, 36,  0,  0,  0, 34, 16,  8,\
                            8, 16, 34,  0,  0, 33, 12,  4,  3,  4, 12, 33,  0], dtype=np.uint8)
        
    def getInstruction(self):
        instruction = [ 19, 10, 10, 0, 1168, 0, 0,\
                        19, 0, 0, 0, 0, 0, 0,\
                        19, 0, 0, 0, 0, 0, 0,\
                        3, 11, 10, 0, 8, 2, 0,\
                        3, 12, 10, 0, 12, 2, 0,\
                        3, 13, 10, 0, 16, 2, 0,\
                        19, 14, 0, 0, 1188, 0, 0,\
                        3, 15, 10, 0, 0, 2, 0,\
                        3, 16, 10, 0, 4, 2, 0,\
                        3, 23, 10, 0, 24, 2, 0,\
                        19, 24, 0, 0, 1, 0, 0,\
                        19, 0, 0, 0, 0, 0, 0,\
                        99, 0, 23, 0, 2053, 0, 0,\
                        3, 23, 10, 0, 24, 2, 0,\
                        19, 26, 17, 0, 2, 0, 0,\
                        19, 0, 0, 0, 0, 0, 0,\
                        19, 0, 0, 0, 0, 0, 0,\
                        99, 0, 23, 26, 2054, 1, 0,\
                        43, 0, 0, 0, 0, 1, 0,\
                        43, 0, 0, 0, 0, 0, 0,\
                        11, 11, 12, 24, 0, 2, 0,\
                        19, 0, 0, 0, 0, 0, 0,\
                        43, 0, 0, 0, 1, 0, 0,\
                        19, 0, 0, 0, 0, 0, 0,\
                        19, 0, 0, 0, 0, 0, 0,\
                        19, 0, 0, 0, 0, 0, 0,\
                        43, 0, 0, 0, 1, 1, 0,\
                        11, 11, 13, 24, 0, 2, 0,\
                        19, 0, 0, 0, 0, 0, 0,\
                        43, 0, 0, 0, 1, 0, 0,\
                        19, 0, 0, 0, 0, 0, 0,\
                        19, 0, 0, 0, 0, 0, 0,\
                        19, 0, 0, 0, 0, 0, 0,\
                        43, 0, 0, 0, 2, 1, 0,\
                        43, 0, 0, 0, 3, 1, 0,\
                        19, 18, 18, 0, 1, 0, 0,\
                        19, 11, 11, 0, 1, 0, 0,\
                        19, 12, 12, 0, 1, 0, 0,\
                        19, 13, 13, 0, 1, 0, 0,\
                        19, 0, 0, 0, 0, 0, 0,\
                        99, 0, 16, 18, 2071, 1, 0,\
                        43, 0, 0, 0, 4, 1, 0,\
                        19, 18, 0, 0, 0, 0, 0,\
                        3, 11, 10, 0, 8, 2, 0,\
                        3, 12, 10, 0, 12, 2, 0,\
                        3, 13, 10, 0, 16, 2, 0,\
                        43, 0, 0, 0, 0, 0, 0,\
                        11, 11, 12, 24, 0, 2, 0,\
                        19, 0, 0, 0, 0, 0, 0,\
                        43, 0, 0, 0, 1, 0, 0,\
                        19, 0, 0, 0, 0, 0, 0,\
                        19, 0, 0, 0, 0, 0, 0,\
                        19, 0, 0, 0, 0, 0, 0,\
                        43, 0, 0, 0, 1, 1, 0,\
                        11, 11, 13, 24, 0, 2, 0,\
                        19, 0, 0, 0, 0, 0, 0,\
                        43, 0, 0, 0, 1, 0, 0,\
                        19, 0, 0, 0, 0, 0, 0,\
                        19, 0, 0, 0, 0, 0, 0,\
                        19, 0, 0, 0, 0, 0, 0,\
                        43, 0, 0, 0, 2, 1, 0,\
                        43, 0, 0, 0, 5, 1, 0,\
                        43, 0, 0, 0, 0, 2, 0,\
                        19, 25, 0, 0, 4, 0, 0,\
                        3, 19, 0, 0, 1152, 2, 0,\
                        3, 20, 0, 0, 1160, 2, 0,\
                        19, 0, 0, 0, 0, 0, 0,\
                        7, 1, 19, 0, 640, 0, 0,\
                        19, 0, 0, 0, 0, 0, 0,\
                        19, 0, 0, 0, 0, 0, 0,\
                        19, 0, 0, 0, 0, 0, 0,\
                        7, 5, 20, 0, 640, 0, 0,\
                        19, 0, 0, 0, 0, 0, 0,\
                        19, 0, 0, 0, 0, 0, 0,\
                        19, 0, 0, 0, 0, 0, 0,\
                        3, 21, 19, 0, 384, 0, 0,\
                        19, 0, 0, 0, 0, 0, 0,\
                        19, 0, 0, 0, 0, 0, 0,\
                        19, 0, 0, 0, 0, 0, 0,\
                        3, 22, 20, 0, 384, 0, 0,\
                        19, 0, 0, 0, 0, 0, 0,\
                        19, 0, 0, 0, 0, 0, 0,\
                        19, 0, 0, 0, 0, 0, 0,\
                        7, 2, 21, 0, 896, 0, 0,\
                        19, 0, 0, 0, 0, 0, 0,\
                        19, 0, 0, 0, 0, 0, 0,\
                        19, 0, 0, 0, 0, 0, 0,\
                        7, 6, 22, 0, 897, 0, 0,\
                        19, 0, 0, 0, 0, 0, 0,\
                        19, 0, 0, 0, 0, 0, 0,\
                        19, 0, 0, 0, 0, 0, 0,\
                        83, 3, 1, 2, 0, 0, 8,\
                        19, 0, 0, 0, 0, 0, 0,\
                        19, 0, 0, 0, 0, 0, 0,\
                        83, 7, 5, 6, 0, 0, 8,\
                        19, 0, 0, 0, 0, 0, 0,\
                        19, 0, 0, 0, 0, 0, 0,\
                        83, 4, 4, 3, 0, 0, 0,\
                        19, 0, 0, 0, 0, 0, 0,\
                        19, 0, 0, 0, 0, 0, 0,\
                        83, 8, 8, 7, 0, 0, 0,\
                        3, 19, 0, 0, 1156, 2, 0,\
                        3, 20, 0, 0, 1164, 2, 0,\
                        19, 0, 0, 0, 0, 0, 0,\
                        7, 1, 19, 0, 640, 0, 0,\
                        19, 0, 0, 0, 0, 0, 0,\
                        19, 0, 0, 0, 0, 0, 0,\
                        19, 0, 0, 0, 0, 0, 0,\
                        7, 5, 20, 0, 640, 0, 0,\
                        19, 0, 0, 0, 0, 0, 0,\
                        19, 0, 0, 0, 0, 0, 0,\
                        19, 0, 0, 0, 0, 0, 0,\
                        3, 21, 19, 0, 384, 0, 0,\
                        19, 0, 0, 0, 0, 0, 0,\
                        19, 0, 0, 0, 0, 0, 0,\
                        19, 0, 0, 0, 0, 0, 0,\
                        3, 22, 20, 0, 384, 0, 0,\
                        19, 0, 0, 0, 0, 0, 0,\
                        19, 0, 0, 0, 0, 0, 0,\
                        19, 0, 0, 0, 0, 0, 0,\
                        7, 2, 21, 0, 896, 0, 0,\
                        19, 0, 0, 0, 0, 0, 0,\
                        19, 0, 0, 0, 0, 0, 0,\
                        19, 0, 0, 0, 0, 0, 0,\
                        7, 6, 22, 0, 897, 0, 0,\
                        19, 0, 0, 0, 0, 0, 0,\
                        19, 0, 0, 0, 0, 0, 0,\
                        19, 0, 0, 0, 0, 0, 0,\
                        83, 3, 1, 2, 0, 0, 8,\
                        19, 0, 0, 0, 0, 0, 0,\
                        19, 0, 0, 0, 0, 0, 0,\
                        83, 7, 5, 6, 0, 0, 8,\
                        19, 0, 0, 0, 0, 0, 0,\
                        19, 0, 0, 0, 0, 0, 0,\
                        83, 4, 4, 3, 0, 0, 0,\
                        19, 0, 0, 0, 0, 0, 0,\
                        19, 0, 0, 0, 0, 0, 0,\
                        83, 8, 8, 7, 0, 0, 0,\
                        19, 0, 0, 0, 0, 0, 0,\
                        19, 0, 0, 0, 0, 0, 0,\
                        83, 4, 4, 8, 0, 0, 4,\
                        19, 18, 18, 0, 1, 0, 0,\
                        19, 11, 11, 0, 1, 0, 0,\
                        39, 0, 14, 4, 0, 2, 0,\
                        19, 12, 12, 0, 1, 0, 0,\
                        19, 13, 13, 0, 1, 0, 0,\
                        11, 0, 14, 25, 0, 0, 0,\
                        83, 4, 0, 0, 0, 0, 0,\
                        19, 0, 0, 0, 0, 0, 0,\
                        19, 0, 0, 0, 0, 0, 0,\
                        83, 8, 0, 0, 0, 0, 0,\
                        19, 0, 0, 0, 0, 0, 0,\
                        19, 0, 0, 0, 0, 0, 0,\
                        99, 0, 16, 18, 2157, 1, 0,\
                        19, 17, 17, 0, 1, 0, 0,\
                        19, 18, 0, 0, 0, 0, 0,\
                        3, 11, 10, 0, 8, 2, 0,\
                        3, 12, 10, 0, 12, 2, 0,\
                        3, 13, 10, 0, 16, 2, 0,\
                        19, 0, 0, 0, 0, 0, 0,\
                        99, 0, 15, 17, 2197, 1, 0,\
                        35, 23, 10, 0, 24, 2, 0,\
                        19, 17, 0, 0, 0, 0, 0,\
                        19, 18, 0, 0, 0, 0, 0,\
                        3, 11, 10, 0, 8, 2, 0,\
                        3, 12, 10, 0, 12, 2, 0,\
                        3, 13, 10, 0, 16, 2, 0,\
                        19, 0, 0, 0, 0, 0, 0,\
                        99, 0, 0, 0, 2209, 0, 0
                        ]
        return instruction

    def getData(self, idxTile, idxCore):
        v = self.leafValue[idxTile, idxCore, :]

        data = np.zeros(2024, dtype=np.uint8)
        data[1168] = self.numBase
        data[1172] = self.numFeature
        data[1176:1176+4] = list(self.offsetCAM.to_bytes(4, byteorder='little'))
        data[1180:1180+4] = list(self.offsetCoreSample.to_bytes(4, byteorder='little'))
        data[1184:1184+4] = list(self.offsetCoreBase.to_bytes(4, byteorder='little'))
        data[self.offsetValue:self.offsetValue + len(v)] = v
        data[self.offsetWeight:self.offsetWeight + len(self.wValue)] = self.wValue

        return data.tolist()

    def getConfig(self):
        config  = {}
        config['instruction'] = self.getInstruction()
        config['data'] = []

        for t in range(self.numTile):
            data_tile = []
            for c in range(self.numCore):
                data_tile.append(self.getData(t,c))
            config['data'].append(data_tile)
        
        return config

class modelConfig():
    def __init__(self, arg, sw):
        self.dataset = arg['dataset']
        self.modelName = arg['modelName']

        temp = np.load(f"./model/{self.dataset}/{self.modelName}.npz")
	self.modelRaw = temp['file1']
        self.modelRaw[:, :-3] = np.round(self.modelRaw[:, :-3], decimals=3)
        self.numFeature = int((np.shape(self.modelRaw)[1]-3)/2)
        self.numClass = int(np.max(self.modelRaw[:, -2])) + 1
        self.numTotalLeaf = np.shape(self.modelRaw)[0]
    
        self.numTest = sw['numTest']
        self.numTestOffset = sw['numTestOffset'] if sw['numTestOffset'] else 0 
        self.numBase = sw['numBase']
        with open(f"./model/{arg['dataset']}/test.pkl", 'rb') as f:
            xy_test = pickle.load(f)
        self.testRaw = np.nan_to_num(xy_test['x'].to_numpy().astype(float))
        self.baseRaw = np.nan_to_num(xy_test['x'].to_numpy().astype(float))

        self.bitQuant = sw['bitQuant']
        self.valueFactor = sw['valueFactor']

        self.noiseP = sw['noiseP']

    def convertFP32toFP8 (self, value):
        # Format (sign, exponent, mantissa)
        # FP32 (1, 8, 23)
        # FP8_E4M3 (1, 4, 3)
        
        fp32_exp = 8
        fp32_man = 23

        fp8_exp = 4
        fp8_man = 3

        fp32_bin = ''.join(f'{c:08b}' for c in struct.pack('!f', value))

        fp32_sign = fp32_bin[0]
        fp32_exponent = fp32_bin[1:fp32_exp+1]
        fp32_mantissa = fp32_bin[fp32_exp+1:]
        
        exponent = int(fp32_exponent, 2) - (2**(fp32_exp-1)-1)
        mantissa = int(fp32_mantissa, 2)
        
        fp8_sign = fp32_sign    
        
        if fp32_exponent == '0'*fp32_exp:
            fp8_exponent = 0
            fp8_mantissa = 0
        if exponent > -7:
            fp8_exponent = exponent + (2**(fp8_exp-1)-1)
            if (mantissa >> (fp32_man-fp8_man-1)) & 0x1:
                fp8_mantissa = (mantissa >> (fp32_man-fp8_man)) + 1
            else:
                fp8_mantissa = mantissa >> (fp32_man-fp8_man)
        elif exponent > -10:
            fp8_exponent = 0
            fp8_mantissa = (2**(fp32_man-(-6-exponent)) + (mantissa>>(-6-exponent))) >> (fp32_man-fp8_man)
        else:
            fp8_exponent = 0
            fp8_mantissa = 0
        
        fp8_bin = fp8_sign + f'{fp8_exponent:0{fp8_exp}b}' + f'{fp8_mantissa:0{fp8_man}b}'
        fp8_int = int(fp8_bin, 2)
        if fp8_exponent == 0:
            fp8_decimal = ((-1)**(int(fp8_sign))) * 2**(-6) * (0+(fp8_mantissa*2**(-fp8_man)))
        else:
            fp8_decimal = ((-1)**(int(fp8_sign))) * 2**(fp8_exponent-(2**(fp8_exp-1)-1)) * (1+(fp8_mantissa*2**(-fp8_man)))
        
        return fp8_int

    def readQuantPkl(self):
        with open(self.fileName, 'rb') as f:
            self.modelConfig = pickle.load(f)
        self.numLeafPerClass = self.modelConfig['info']['numLeafPerClass']

    def quantizeThreshold(self):
        quant_level = 2**self.bitQuant
        
        th_low = np.zeros((self.numTotalLeaf, self.numFeature))
        th_high = np.zeros((self.numTotalLeaf, self.numFeature))
        th_lowX = np.zeros((self.numTotalLeaf, self.numFeature))
        th_highX = np.zeros((self.numTotalLeaf, self.numFeature))
        th_list = {}
        
        for i in range(self.numFeature):
            th_unique = np.unique(self.modelRaw[:, 2*i:(2*i+2)])
            th_unique = th_unique[~np.isnan(th_unique)]
            th_list[i] = th_unique
                             
            th_low[:, i] = np.digitize(self.modelRaw[:, 2*i], th_unique, right=True)
            th_high[:, i] = np.digitize(self.modelRaw[:, 2*i+1], th_unique, right=True)
            th_lowX[:, i] = (~np.isnan(self.modelRaw[:,2*i]))
            th_highX[:, i] = (~np.isnan(self.modelRaw[:,2*i+1]))


        noise = np.random.choice(a = [-1, 0, 1], size = (2, self.numTotalLeaf, self.numFeature), p = [self.noiseP, 1-2*self.noiseP, self.noiseP])
        th_low = np.minimum(255, np.maximum(0, th_low + noise[0, :, :]))
        th_high = np.minimum(255, np.maximum(0, th_high + noise[1, :, :]))

        th_low = th_low.astype(np.uint8)
        th_high = th_high.astype(np.uint8)
        th_lowX = th_lowX.astype(np.uint8)
        th_highX = th_highX.astype(np.uint8)
        
        self.modelConfig['modelThList'] = th_list
        self.numLeafPerClass = np.zeros(self.numClass)
        for c in range(self.numClass):
            th_dict = {}
            th_dict['modelThLow'] = th_low[self.modelRaw[:, -2] == c]
            th_dict['modelThHigh'] = th_high[self.modelRaw[:, -2] == c]
            th_dict['modelThXLow'] = th_lowX[self.modelRaw[:, -2] == c]
            th_dict['modelThXHigh'] = th_highX[self.modelRaw[:, -2] == c]
            self.modelConfig[c] = th_dict
            self.numLeafPerClass[c] = np.shape(th_dict['modelThLow'])[0]

    def quantizeValue(self):
        
        for c in range(self.numClass):
            model_c_class = self.modelRaw[self.modelRaw[:, -2] == c]
            model_row = np.shape(model_c_class)[0]
            value_raw = np.copy(model_c_class[:, -3])
            value_fp8_int = np.zeros(model_row)
            
            for i in range(model_row):
                value_fp8_int[i] = self.convertFP32toFP8(self.valueFactor*value_raw[i])
                
            value_fp8_int = value_fp8_int.astype(np.uint8)
            self.modelConfig[c]['value'] = value_fp8_int
    def quantizeInput(self):
        testQuant = np.zeros_like(self.testRaw)
        baseQuant = np.zeros_like(self.baseRaw)
        th_list = self.modelConfig['modelThList']
        for i in range(self.numFeature):
            testQuant[:, i] = np.digitize(self.testRaw[:, i], th_list[i], right=False)
            baseQuant[:, i] = np.digitize(self.baseRaw[:, i], th_list[i], right=False)
        testQuant = testQuant.astype(np.uint8)
        baseQuant = baseQuant.astype(np.uint8)
        self.modelConfig['testSample'] = testQuant[self.numTestOffset:self.numTestOffset+self.numTest, :]
        self.modelConfig['baseSample'] = baseQuant[:self.numBase, :]

    def quantizeWeight(self, max_depth = 8):
        weight_fp8_int = np.zeros(max_depth*max_depth, dtype=np.uint8)
        
        for n in range(max_depth):
            for s in range(n):
                temp = math.factorial(s)*math.factorial(n-s-1)/math.factorial(n)
                weight_fp8_int[n*8+s] = self.convertFP32toFP8(temp)
        
        self.modelConfig['weight'] = weight_fp8_int

    def quantize(self):
        self.modelConfig = {}
        self.quantizeThreshold()
        self.quantizeValue()
        self.quantizeInput()
        self.quantizeWeight()
        self.modelConfig['info'] = {}
        self.modelConfig['info']['numFeature'] = self.numFeature
        self.modelConfig['info']['numClass'] = self.numClass
        self.modelConfig['info']['numLeafPerClass'] = self.numLeafPerClass
        with open(self.fileName, 'wb') as f:
            pickle.dump(self.modelConfig, f)

    def getConfig(self, overwrite = False):
        self.overwrite = overwrite
        self.fileName = f"./model/{self.dataset}/{self.modelName}_quant{int(self.bitQuant)}_v{self.valueFactor}_p{self.noiseP*100}.pkl"
        if not (overwrite) and (os.path.isfile(self.fileName)):
            print("Already have quantized model/input/weights.")
            self.readQuantPkl()
        else:
            self.quantize()

        self.modelConfig['info'] = {}
        self.modelConfig['info']['numFeature'] = self.numFeature
        self.modelConfig['info']['numClass'] = self.numClass
        self.modelConfig['info']['numLeafPerClass'] = self.numLeafPerClass

        return self.modelConfig


class acamConfig():
    def __init__(self, hwConfig, swConfig, modelConfig):
        # Model info
        self.numFeature = modelConfig['info']['numFeature']
        self.numClass = modelConfig['info']['numClass']
        self.numLeafPerClass = modelConfig['info']['numLeafPerClass']

        # Model threshold/value/input data/weight
        self.modelConfig = modelConfig

        # Hardware info        
        self.numTile = pow(hwConfig['node']['numPort'], hwConfig['node']['numLevel'])
        self.numCore = hwConfig['node']['numCore']
        self.numTotalCore = self.numTile*self.numCore
        self.numRow = hwConfig['node']['numRow']
        self.numCol = hwConfig['node']['numCol']

        self.gBit = hwConfig['acam']['param']['gBit']
        self.gMax = hwConfig['acam']['param']['gMax']
        self.gMin = hwConfig['acam']['param']['gMin']
        
        # Software info
        swConfig['beginCore'].append(self.numTotalCore)
        self.beginCore = swConfig['beginCore']

    def getConfig(self):

        acamThLow = np.zeros((self.numTile, self.numCore, self.numRow, self.numCol))
        acamThHigh = np.zeros((self.numTile, self.numCore, self.numRow, self.numCol))
        acamThXLow = np.zeros((self.numTile, self.numCore, self.numRow, self.numCol))
        acamThXHigh = np.zeros((self.numTile, self.numCore, self.numRow, self.numCol))
        acamValue = np.zeros((self.numTile, self.numCore, self.numRow))

        tilePerClass = np.zeros((self.numClass, 2))

        numColX = int(self.numCol - self.numFeature)
        
        for c in range(self.numClass):
            indexCore = 0
            modelConfigC = self.modelConfig[c]
            numCorePerClass = self.beginCore[c+1]-self.beginCore[c]
            numRowX = int(numCorePerClass*(self.numRow-1) - self.numLeafPerClass[c])
            if numRowX < 0:
                modelThLow = np.pad(modelConfigC['modelThLow'][:numCorePerClass*(self.numRow-1), :], ((0, 0), (0, numColX)), 'constant', constant_values=0)
                modelThHigh = np.pad(modelConfigC['modelThHigh'][:numCorePerClass*(self.numRow-1), :], ((0, 0), (0, numColX)), 'constant', constant_values=0)
                modelThXLow = np.pad(modelConfigC['modelThXLow'][:numCorePerClass*(self.numRow-1), :], ((0, 0), (0, numColX)), 'constant', constant_values=0)
                modelThXHigh = np.pad(modelConfigC['modelThXHigh'][:numCorePerClass*(self.numRow-1), :], ((0, 0), (0, numColX)), 'constant', constant_values=0)
                modelValue = modelConfigC['value'][:numCorePerClass*(self.numRow-1)]
            else:
                modelThLow = np.pad(modelConfigC['modelThLow'], ((0, numRowX), (0, numColX)), 'constant', constant_values=0)
                modelThHigh = np.pad(modelConfigC['modelThHigh'], ((0, numRowX), (0, numColX)), 'constant', constant_values=0)

                modelThXLow = np.pad(modelConfigC['modelThXLow'], ((0, numRowX), (0,0)), 'constant', constant_values=1)
                modelThXHigh = np.pad(modelConfigC['modelThXHigh'], ((0, numRowX), (0,0)), 'constant', constant_values=1)
                modelThXLow = np.pad(modelThXLow, ((0,0), (0, numColX)), 'constant', constant_values=0)
                modelThXHigh = np.pad(modelThXHigh, ((0,0), (0, numColX)), 'constant', constant_values=0)

                modelValue = np.pad(modelConfigC['value'], (0, numRowX), 'constant', constant_values=0)

            modelThLow = np.reshape(modelThLow, (numCorePerClass, (self.numRow-1), self.numCol))
            modelThHigh = np.reshape(modelThHigh, (numCorePerClass, (self.numRow-1), self.numCol))
            modelThXLow = np.reshape(modelThXLow, (numCorePerClass, (self.numRow-1), self.numCol))
            modelThXHigh = np.reshape(modelThXHigh, (numCorePerClass, (self.numRow-1), self.numCol))
            modelValue = np.reshape(modelValue, (numCorePerClass, (self.numRow-1)))

            
            beginTile = int(np.floor(self.beginCore[c]/self.numCore))
            endTile = int(np.floor((self.beginCore[c+1]-1)/self.numCore))

            tilePerClass[c, 0] = beginTile
            tilePerClass[c, 1] = endTile
            for t in range(beginTile, endTile):
                beginCore = max(int(self.beginCore[c]), int(t*self.numCore)) - t*self.numCore
                endCore = min(int(self.beginCore[c+1]), int((t+1)*self.numCore)) - t*self.numCore

                acamThLow[t, beginCore:endCore, :(self.numRow-1), :] = modelThLow[indexCore:indexCore+(endCore-beginCore), :, :]
                acamThHigh[t, beginCore:endCore, :(self.numRow-1), :] = modelThHigh[indexCore:indexCore+(endCore-beginCore), :, :]
                acamThXLow[t, beginCore:endCore, :(self.numRow-1), :] = modelThXLow[indexCore:indexCore+(endCore-beginCore), :, :]
                acamThXHigh[t, beginCore:endCore, :(self.numRow-1), :] = modelThXHigh[indexCore:indexCore+(endCore-beginCore), :, :]
                acamValue[t, beginCore:endCore, :(self.numRow-1)] = modelValue[indexCore:indexCore+(endCore-beginCore), :]
                indexCore = indexCore+(endCore-beginCore)

        config = {}
        config['acamThLow'] = acamThLow
        config['acamThHigh'] = acamThHigh
        config['acamThXLow'] = acamThXLow
        config['acamThXHigh'] = acamThXHigh
        config['acamValue'] = acamValue
        
        gLevel = 2**self.gBit
        gDelta = (self.gMax-self.gMin)/gLevel
        gList = gDelta*np.arange(gLevel) + self.gMin
        config['gList'] = gList
        return config, tilePerClass

class jsonConfig():
    def __init__(self, name, path="./json/"):
        self.name = name

        fileName = path+"{}.json".format(name)
        try:
            with open(fileName, 'r') as f:
                self.config = json.load(f)
        except Exception as err:
            sys.exit('FATAL: could not import `{}` - {}'.format(fileName, err) )

    def getName(self):
        return self.name
    def getConfig(self):
        return self.config

class variationConfig():
    def __init__(self, folder, numTrial):
        self.hw = jsonConfig("hw")
        self.sw = jsonConfig("sw")
        self.folder = folder
        self.numTrial = numTrial
    
    def writeJsonConfig(self):
        hw_default = copy.deepcopy(self.hw.getConfig())
        sw_default = copy.deepcopy(self.sw.getConfig())

        benchPath = "./log/{0}/".format(self.folder)
        if not (os.path.isdir(benchPath)):
            os.makedirs(benchPath)
            
        for j in range(self.numTrial):
            jsonPath = benchPath + "{0:d}/".format(j)
            rawPath = jsonPath+"raw/"
            if not (os.path.isdir(rawPath)):
                os.makedirs(rawPath)
        
            sw_default["logDir"] = rawPath
            with open(jsonPath + "hw_{:d}.json".format(j), "w") as hwfile: 
                json.dump(hw_default, hwfile, indent = 4)
            with open(jsonPath + "sw_{:d}.json".format(j), "w") as swfile: 
                json.dump(sw_default, swfile, indent = 4)

class sstConfig():
    def __init__(self, simName, folder):
        self.simName = simName
        self.path = "./log/{0}/{1:d}/".format(folder, simName)
        if not (os.path.isdir(self.path)):
            os.makedirs(self.path+"raw/")
            print("Use default hw and sw at './json/' folder.\n")
            self.hw = jsonConfig("hw")
            self.sw = jsonConfig("sw")
            self.sw.getConfig()['logDir'] = self.path+"raw/"
            with open(self.path + "hw_{:d}.json".format(self.simName), "w") as hwfile: 
                json.dump(self.hw.getConfig(), hwfile, indent = 4)
            with open(self.path + "sw_{:d}.json".format(self.simName), "w") as swfile: 
                json.dump(self.sw.getConfig(), swfile, indent = 4)
        else:
            self.hw = jsonConfig("hw_{}".format(self.simName), self.path)
            self.sw = jsonConfig("sw_{}".format(self.simName), self.path)

def readArg():
    ap = argparse.ArgumentParser()
    ap.add_argument("--dataset", type=str, default="churn", help="Name of dataset")
    ap.add_argument("--model", type=str, default="xgboost_tree800_depth10_trial0", help="Name of tree-based ML model in ./model/DATASET/ folder")
    ap.add_argument("--folder", type=str, default="churn_f100b100_v10_p0", help="Name of simulation result folder in ./log/ folder")
    args = ap.parse_args()

    arg = {}
    arg['dataset'] = args.dataset
    arg['modelName'] = args.model
    arg['folder'] = args.folder

    return arg



