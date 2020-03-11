set title "BxDecay0 - Example ex05"

Qbb = 3.0
set grid
# set key out

set xlabel "E_1 (MeV)"
set ylabel "Count"
plot [0:2000][0:Qbb+1]\
	'bxdecay0_ex05_1.data' title "Electron 1 energy" 

pause -1 "Hit [Enter]..."
set term push
set terminal jpeg
set output "bxdecay0_ex05_e1.jpg"
replot
set output
set terminal pop


set xlabel "E_2 (MeV)"
set ylabel "Count"
plot [0:2000] [0:Qbb+1] \
     'bxdecay0_ex05_2.data' title "Electron 2 energy" 
pause -1 "Hit [Enter]..."
set term push
set terminal jpeg
set output "bxdecay0_ex05_e2.jpg"
replot
set output
set terminal pop

set xlabel "E_{sum} (MeV)"
set ylabel "Count"
plot [0:2000][0:4.0] 'bxdecay0_ex05_3.data' title "E_1 + E_2"
pause -1 "Hit [Enter]..."
set term push
set terminal jpeg
set output "bxdecay0_ex05_esum.jpg"
replot
set output
set terminal pop

set xlabel "cos_{12}"
set ylabel "Counts / 0.02"
plot [0:1.2][0:] 'bxdecay0_ex05_4.data'title "Random cos12" 
pause -1 "Hit [Enter]..."
set term push
set terminal jpeg
set output "bxdecay0_ex05_cos12.jpg"
replot
set output
set terminal pop

# end

