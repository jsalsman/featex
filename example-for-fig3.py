#!/usr/bin/env python2
# -*- coding: utf-8 -*-
"""
Created on Sun Aug 20

@author: jsalsman
"""

from keras.models import Sequential
from keras.layers import Dense, Dropout
from keras.utils.np_utils import to_categorical
from numpy import asarray
from scipy.stats import rankdata

lines = []
with open('featex-tran.txt', 'r') as f: # or, phrase-sliced.txt which has some
                                        # overlapping words so maybe rename
                                        # one of them if you want to use the
                                        # same server for the phrase as for
                                        # the 82 words
    lines.extend(f.readlines())

X = [] # testing data independents
y = [] # testing data dependents
word = '' # word name
model = {}
n = 0
layers = 4
units = 32
epochs = 1000
drop = 0.25
features = None

for line in lines + ['.']:
    tokens = line.strip().split()
    if line[0] != ' ': # new word
        if word != '': # not the first word
            print ("word:", word, n, "transcripts,", features, "features")

            y_cat = to_categorical(y)
            model[word].fit(X, y_cat, epochs=epochs, verbose=0)

            # now you can get the probability of intelligibility for some
            # featex vector Z this way:
            # pi = model[word].predict(asarray(Z).reshape(1, -1))[0][1]

        if line != '.': # not the last line
            word = tokens[2]
            features = int(tokens[4]) * int(tokens[6]) + int(tokens[8])
            X = []; y = []; n = 0

            model[word] = Sequential() # DNN
            model[word].add(Dense(units, input_dim=features,
                            activation='softmax',
                            kernel_initializer='glorot_uniform'))
            model[word].add(Dropout(drop))
            for i in range(layers):
                model[word].add(Dense(units,
                                      kernel_initializer='glorot_uniform'))
                model[word].add(Dropout(drop))
            model[word].add(Dense(2, activation='softmax',
                            kernel_initializer='glorot_uniform'))
            model[word].compile(optimizer='adam',
                                loss='categorical_crossentropy')

    else: # read a transcription's word data observation
        if len(tokens) > features + 1: # ignore incomplete recognition results
            fvec = []
            for i in range(features):
                fvec.append(float(tokens[i + 2]))
            if tokens[1] == "<-":
                X.append(fvec)
                y.append(float(tokens[0]))
                n += 1


because_00766t_2 = [0.22, 0.178, 0.929, 0.744, 0.05, 0.200, 0.381, 0.981,
                    0.05, 0.182, 0.548, 0.000, 0.25, 0.161, 0.786, 0.512,
                    0.43, 0.150, 0.929, 0.869, 0.725]
# unintelligible, pronounced "cuz" without the "bee-"
# 0.049262498

because_01004t_5 = [0.08, 0.277, 1.000, 0.000, 0.09, 0.275, 0.976, 0.000,
                    0.11, 0.261, 0.952, 0.988, 0.06, 0.198, 0.929, 1.000,
                    0.05, 0.181, 0.333, 0.919, 0.569]
# intelligible, pronounced "because-ah" as in a typical Chinese primary ESL student accent
# 0.57494467

def perturb(V, word):
    print(model[word].predict(asarray(V).reshape(1, -1))[0][1])
    phonemes = (len(V) - 1) // 4
    pbs = []
    for n in range(phonemes):
        Z = list(V)
        Z[n*4 + 1] *= 1.5
        Z[n*4 + 2] *= 1.5
        Z[n*4 + 3] *= 1.5
        p_i = model[word].predict(asarray(Z).reshape(1, -1))[0][1]
        print(p_i)
        pbs.append(p_i)
    return [int(i) for i in rankdata(pbs)]

model['because'].predict(asarray([because_00766t_2]).reshape(1, -1))[0][1]
# 0.049262498

model['because'].predict(asarray([because_01004t_5]).reshape(1, -1))[0][1]
# 0.57494467

perturb(because_00766t_2, 'because')
