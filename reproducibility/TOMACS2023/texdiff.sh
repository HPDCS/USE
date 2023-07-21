#!/bin/bash

unzip -q -d ./tomacs-tex-files "TOMACS-Spatial and temporal locality-2023.zip"

cd tomacs-tex-files

latexdiff ./backup/versione-prima-sottomissione.tex revisione.tex > diff.tex 
#pdflatex ./diff.tex