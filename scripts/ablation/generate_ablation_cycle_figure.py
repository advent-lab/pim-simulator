import os, glob
import sys
import re
import numpy as np
import matplotlib
import matplotlib.pyplot as plot
import matplotlib.gridspec as gridspec
import subprocess, os


print('Number of arguments:', len(sys.argv), 'arguments.')
print('Argument List:', str(sys.argv))

import csv
import sum_network

header = ['workload_name', 'All', 'minus Htree, plus Bus', 'minus constant ops', 'minus cross cram shift', 'minus systolic broadcast, plus one-to-all broadcast', 'minus shuffle units', 'minus mesh, plus ring', 'minus Htree, constant ops, cross cram shift, sys-bcast, shuffle, mesh']
data = ['', 0, 0, 0, 0, 0, 0, 0]

f_fig = open(os.path.join('04_upload_ablation_cycle_figure.csv'), 'w', encoding='UTF8', newline='')
writer = csv.writer(f_fig)
# write the header
writer.writerow(header)


fig_data_list = [['conv', 0, 0, 0, 0, 0, 0, 0, 0],
                    ['fir', 0, 0, 0, 0, 0, 0, 0, 0],
                    ['gemm', 0, 0, 0, 0, 0, 0, 0, 0],
                    ['gemv', 0, 0, 0, 0, 0, 0, 0, 0],
                    ['vadd', 0, 0, 0, 0, 0, 0, 0, 0],
                    ['resnet18', 0, 0, 0, 0, 0, 0, 0, 0],
                    ['bert', 0, 0, 0, 0, 0, 0, 0, 0]
]
assert os.path.isfile(os.path.join(os.getcwd(),'output_all_kernels', '03_chart_breakdown_avg.csv')), "chart_breakdown_avg.csv does not exist. Run collect_stats.py and generate_chart_data.py first."
all_kernels_input_file_dir = os.path.join(os.getcwd(),'output_all_kernels')
with open(os.path.join(os.getcwd(), all_kernels_input_file_dir, '03_chart_breakdown_avg.csv'), 'r') as f:

    csv_reader = csv.reader(f)
    for row in csv_reader:
        workload_name = row[0]
        total_cycle = row[5]
        col=0
        if('bus' in workload_name): col = 2
        elif('constOpOff' in workload_name): col=3
        elif('crossCramShiftOff' in workload_name): col=4
        elif('oneToAllBroadcast' in workload_name): col=5
        elif('shuffleOff' in workload_name): col=6
        elif('everythingOff' in workload_name): col=8
        elif('ring' in workload_name): col=7
        else: col=1
        row=0
        if('conv2d' in workload_name): row=0
        elif('fir' in workload_name): row=1
        elif('gemm' in workload_name): row=2
        elif('gemv' in workload_name): row=3
        elif('vecadd' in workload_name): row=4
        else: continue
        fig_data_list[row][col]=total_cycle



def fill_data(fig_data_list, workload_name, input_file_dir, row):
    fig_data_list[row][2]=sum_network.get_time(workload_name, input_file_dir, 'bus')
    fig_data_list[row][3]=(sum_network.get_time(workload_name, input_file_dir,'constOpOff')+sum_network.get_time(workload_name, input_file_dir,'constOpOff_kickoutRow'))/2
    fig_data_list[row][4]=(sum_network.get_time(workload_name, input_file_dir,'crossCramShiftOff')+sum_network.get_time(workload_name, input_file_dir,'crossCramShiftOff_bypassDram'))/2
    fig_data_list[row][5]=sum_network.get_time(workload_name, input_file_dir,'oneToAllBroadcast')
    fig_data_list[row][6]=sum_network.get_time(workload_name, input_file_dir,'shuffleOff')
    fig_data_list[row][8]=sum_network.get_time(workload_name, input_file_dir,'everythingOff')
    fig_data_list[row][7]=sum_network.get_time(workload_name, input_file_dir,'ring')
    fig_data_list[row][1]=sum_network.get_time(workload_name, input_file_dir,'all')
    # print(fig_data_list[row])


assert os.path.isfile(os.path.join(os.getcwd(),'output_resnet', '00_stats.csv')), "00_stats.csv does not exist. Run collect_stats.py and generate_chart_data.py first."

fill_data(fig_data_list, 'resnet', 'output_resnet', 5)
fill_data(fig_data_list, 'bert', 'output_bert', 6)
        

for row in fig_data_list:
    writer.writerow(row)

# Plot
data = np.array([ele[1:] for ele in fig_data_list])
print(data)

matplotlib.rcParams['lines.linewidth'] = 2.5

plot.style.use('bmh')
binary = matplotlib.cm.get_cmap('binary')

fig, axes = plot.subplots(1,7,figsize=(18,4.5))
# data = np.array([[20713,	58112,	20713,	20713,	105211,	58138,	156003,48828],\
#                     [27336,	28464,	48924,	681138,	27290,	27336,	724868,27336],\
#                     [146310,	471659,	146310,	146310,	148416,	210767,	535777,396395],\
#                     [117171,	137319,	129064,	117171,	117171,	117171,	144257,117171],\
#                     [30944,	30944,	30944,	30944,	30944,	30944,	30944,30944]])
# print(data)
data = data.transpose()
# (a) shuffle disabled
# (b) Const Ops disabled
# (c) H-Tree disabled
# (d) Cross-CRAM Shift disabled
# (e) Systolic Bcast disabled
# (f) All disabled
# (g) IntraCRAM Reduction
# data = np.array([data[5], data[2], data[1], data[3], data[4], data[6],data[7],data[0]])
# print(data)

to_gm = []
to_label = []
colors = ['0.2', '0.4', '0.6', '0.8', '1.0']
hatches = ['', '', '////','\\\\\\\\','----']


for i, ty in enumerate(['(a) shuffle disabled', '(b) Const Ops disabled', '(c) H-Tree disabled', '(d) Cross-CRAM \n Shift disabled', '(e) Systolic Bcast \n disabled', '(f) All disabled','(g) IntraCRAM Reduction']):
    b=data[7]/data[i]
    print(b)
    axs = axes[i]
    #axes.bar(np.arange(0, 5) * 2.5 + 0, a, bottom=a_acc, width=1, edgecolor='k', color=binary(0.3 * i + 0.1), label=ty)
    axs.bar(np.arange(0, 5) * 1.9, b, bottom=0, width=1, edgecolor='k', color=colors, hatch=hatches[0])
    axs.set_ylim(0, 1)
    axs.set_xlim(-1, 8.5)

    # axs.legend(bbox_to_anchor=(0.3, 1.25), handlelength=0.75, ncol=4, loc='upper center',
    #         labelspacing=0.2, handletextpad=0.5, columnspacing=0.5, frameon=False,
    #         fontsize=10)
    axs.set_xticks(np.arange(0, 5) * 1.9)
    axs.set_yticks([0, 0.2, 0.4, 0.6, 0.8, 1])
    axs.set_yticklabels([str(20 * i) + '%' for i in range(6)],fontsize=12)
    if(i!=0): axs.tick_params(left = False, right = False , labelleft = False , labelright = False)
    #axes.set_yticks([0.1 * i for i in range(12)])
    #axes.set_yticklabels([str(10 * i) + '%' for i in range(12)])
    axs.set_xticklabels(['conv2d', 'fir', 'gemm', 'gemv', 'vadd'], rotation=90,fontsize=15)
    axs.xaxis.grid(False)
    axs.set_axisbelow(True)
    axs.set_title(ty, fontsize=13, wrap=True)
    
    #axes.text(0.45, 1.15, 'Exec. Time Breakdown', fontsize=14)
    # axs.set_ylabel('Exec. Time Brkd.')

    # fig.subplots_adjust(top=0.5, bottom=0.2, left=0.2, right=0.45, wspace=0.02)

fig.subplots_adjust(bottom=0.2)
plot.show()
fname = 'hdw-feature-ablation'
fig.savefig(f'{fname}.pdf')
subprocess.check_output(f'pdfcrop {fname}.pdf', shell=True)
subprocess.check_output(f'mv {fname}-crop.pdf {fname}.pdf', shell=True)