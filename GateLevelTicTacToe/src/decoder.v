module decoder(input[1:0]i,output[3:0]o);

wire w1;
wire n1,n2;

or ott1(w1,i[0],i[1]);//w1=i[0].i[1]
not ott1c(o[0],w1);//o[0]=not w1
and dtt1(o[3],i[0],i[1]);//o[3]=i[0].i[1]
not i1c(n1,i[1]); //n1=i[1]'
and dtt2(o[1],n1,i[0]); //o[1]
not i0c(n2,i[0]);//i[0]'
and dtt3(o[2],n2,i[1]);//o[2]

endmodule