
module up_counter(
  input clk,
  input rst,
  input [2:0] foo
);

reg[2:0] val = 1;

assign foo = val;


always @(posedge clk)
begin

	if (rst) begin
		val <= 0;
	end else begin
		val <= val + 1;
	end
end


endmodule 
