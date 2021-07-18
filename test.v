module up_counter(
  input clk,
  input rst
  , input [7:0] foo
 );

 reg[2:0] val;
 
#if 0
 always @(posedge clk)
 begin

 /*
  if (rst) begin
    val <= 3'b0;
  end else begin
    val <= val + 1;
  end
  */
    val <= val + 1;

 end
 endmodule 
#endif