`timescale 1 ps/ 1 ps
module al422_bam_bs_vlg_tst();
// test vector input registers
reg in_clk;
reg [15:0] address;
reg [7:0] memory [0:65535];
reg in_nrst;
// wires                                               
wire al422_nrst;
wire al422_re;
wire cntr_nrst;
wire led_clk_out;
wire led_lat_out;
wire led_oe_out;
wire [4:0]  led_row;
wire [2:0]  rgb1;
wire [2:0]  rgb2;

wire [7:0] memory_out;

assign cntr_nrst = al422_nrst & in_nrst;
assign memory_out = memory[address];

al422_bam_bs al422_bam_bs (
// port map - connection between master ports and signals/registers   
	.al422_nrst_out(al422_nrst),
	.al422_re_out(al422_re),
	.in_clk(in_clk),
	.in_data(memory_out),
	.in_nrst(in_nrst),
	.led_clk_out(led_clk_out),
	.led_lat_out(led_lat_out),
	.led_oe_out(led_oe_out),
	.led_row(led_row),
	.rgb1(rgb1),
	.rgb2(rgb2)
);

integer j, ca;
integer n_File_ID;
integer n_Temp;

initial
begin
	for (j=0; j < 8192; j=j+1)
		memory[j] <= 8'h00; //reset array

	ca = 0;
	for (j=0; j < 10; j=j+1)
	begin
		memory[ca] <= 8'b00101111; // oe inverted, row F
		memory[ca+1] <= 8'h00; // active low
		memory[ca+2] <= 8'h00; // active high
		memory[ca+3] <= 8'h00; // inactive low
		memory[ca+4] <= 8'h00; // inactive high
		memory[ca+5] <= 8'h31;
		memory[ca+6] <= 8'h32;
		memory[ca+7] <= 8'h33;
		memory[ca+8] <= 8'h34;
		memory[ca+9] <= 8'h35;
		memory[ca+10] <= 8'h36;
		if (j==9)
			memory[ca+11] <= 8'h74;
		else
			memory[ca+11] <= 8'h44;
		ca = ca + 12;
	end
	
	/*
	n_File_ID = $fopen("dump.bin", "rb");
	n_Temp = $fread(memory, n_File_ID);
	$fclose(n_File_ID);
	*/

	in_nrst = 0;
	in_clk = 0;                                                       
	address = 0;
	#15 in_nrst = 1;
	#2000000 $stop;
end                                                    

always                                                 
  #10 in_clk = ~in_clk;
  
always @ (posedge in_clk)
	if (!al422_re)
	begin
		if (~cntr_nrst)
			address <= 16'h0;
		else
			address <= address + 16'h1;
	end

endmodule

