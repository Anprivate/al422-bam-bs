/*
	packet:
	byte 0 - 4:0 - current row, bit 5 - oe inverted, bit 6 - lat inverted, bit 7 - clk inverted
	bytes 1-2 - OE active counter preload (16 bit, intel order)
	bytes 3-4 - OE passive counter preload (16 bit, intel order)
	bytes 5-n - output RGB data. 5:0 - rgb_data as is, 6 - end of block, 7 - end of frame (address counter reset)
*/

module al422_bam_bs (
	input wire in_clk, in_nrst,
	// al422 pins
	input wire [7:0] in_data,
	output wire al422_nrst_out, al422_re_out,
	// led outpur pins HUB75E
	output wire led_clk_out, led_oe_out, led_lat_out,
	// up to 1/32 scan
	output wire [4:0] led_row,
	// up to 2 RGB outputs
	output wire [2:0] rgb1, rgb2
);

	reg led_oe, led_lat, led_clk;
	reg [2:0] out_phases;
	
	assign led_oe_out = led_oe ^ out_phases[0];
	assign led_lat_out = led_lat ^ out_phases[1];
	assign led_clk_out = led_clk ^ out_phases[2];

	reg [5:0] rgb_data;
	assign rgb1 = rgb_data[2:0];
	assign rgb2 = rgb_data[5:3];
	
	reg [4:0] cur_row;
	assign led_row = cur_row;
	
	reg [15:0] oe_counter;
	reg [15:0] oe_inactive_register;
	
	reg [2:0] phase_counter;
	
	always @(posedge in_clk or negedge in_nrst)
		if (~in_nrst)
			phase_counter <= 3'b0;
		else
			case (phase_counter)
				3'h0: phase_counter <= 3'h1;
				3'h1: phase_counter <= 3'h2;
				3'h2: phase_counter <= 3'h3;
				3'h3: phase_counter <= 3'h4;
				3'h4: phase_counter <= 3'h5;
				3'h5: if (oe_counter_is_zero)
					phase_counter <= 3'h6;
				3'h6: if (oe_counter_is_zero)
					phase_counter <= 3'h7;
			endcase
			//if ((phase_counter < 3'h5) | ((phase_counter == 3'h5) & oe_active_is_zero) | ((phase_counter == 3'h6) & oe_inactive_is_zero))
//			if ((phase_counter < 3'h5) | ((phase_counter == 3'h5) & oe_active_is_zero))
//				phase_counter <= phase_counter + 1'b1;

	always @(posedge in_clk or negedge in_nrst)
		if (~in_nrst)
			cur_row <= 5'b0;
		else
			if (phase_counter == 3'h0)
				cur_row <= in_data[4:0];
	
	always @(posedge in_clk or negedge in_nrst)
		if (~in_nrst)
			out_phases <= 3'b0;
		else
			if (phase_counter == 3'h0)
				out_phases <= in_data[7:5];
	
	wire oe_counter_is_zero;
	assign oe_counter_is_zero = (oe_counter == 16'h0);
	
	always @(posedge in_clk or negedge in_nrst)
		if (~in_nrst)
			oe_counter <= 16'h0;
		else
		begin
			if (phase_counter == 3'h1)
				oe_counter[7:0] <= in_data;
			if (phase_counter == 3'h2)
				oe_counter[15:8] <= in_data;
			if ((phase_counter == 3'h5) | (phase_counter == 3'h6))
				if (oe_counter_is_zero)
					oe_counter <= oe_inactive_register;
				else
					oe_counter <= oe_counter - 1'b1;
		end
			
	always @(posedge in_clk or negedge in_nrst)
		if (~in_nrst)
			oe_inactive_register <= 16'h0;
		else
		begin
			if (phase_counter == 3'h3)
				oe_inactive_register[7:0] <= in_data;
			if (phase_counter == 3'h4)
				oe_inactive_register[15:8] <= in_data;
		end
			
	always @(posedge in_clk or negedge in_nrst)
		if (~in_nrst)
			rgb_data <= 6'h0;
		else
			rgb_data <= in_data;
endmodule
