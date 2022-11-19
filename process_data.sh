mkdir -p data
mkdir -p figures


./base_scripts/figure5_gen.sh
./base_scripts/figure6_gen.sh
./base_scripts/figure8_gen.sh
./base_scripts/figure9_gen.sh


cd figures
pdflatex merge.tex
cd ..
