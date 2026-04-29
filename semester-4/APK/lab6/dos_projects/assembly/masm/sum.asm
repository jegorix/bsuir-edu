.MODEL SMALL 
.STACK 100h 
.DATA   
              
a dw 5 
b dw 6 
result dw ? 
 
.CODE                 
START: 
 
mov ax, @data    
mov ds, ax 
 
mov ax, a 
add ax, b 
mov result, ax 
 
mov ax, 4C00h 
int 21h 
END START    
