transcript on
if {[file exists rtl_work]} {
	vdel -lib rtl_work -all
}
vlib rtl_work
vmap work rtl_work

vlog -vlog01compat -work work +incdir+P:/altera/al422-bam-bs {P:/altera/al422-bam-bs/al422_bam_bs.v}

vlog -vlog01compat -work work +incdir+P:/altera/al422-bam-bs {P:/altera/al422-bam-bs/al422_bam_bs_tb.v}

vsim -t 1ps -L altera_ver -L lpm_ver -L sgate_ver -L altera_mf_ver -L altera_lnsim_ver -L max_ver -L rtl_work -L work -voptargs="+acc"  al422_bam_bs_vlg_tst

add wave *
view structure
view signals
run -all
