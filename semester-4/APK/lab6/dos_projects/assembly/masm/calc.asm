.MODEL SMALL 
.STACK 100h 
.DATA 
MESSAGE db "((X+Y) 8 - X) / 2 = $" 
X dw 06h 
Y dw 07h 
Z dw ? 
.CODE 
START: 
mov ax, @data 
mov ds, ax 
 
mov ax, X 
add ax,Y 
mov bx, 8 
mul  bx 
sub ax, X 
mov bx, 2 
mov dx, 0 
div bx 
mov Z, ax 
mov ah, 09h   
mov dx, offset MESSAGE 
int 21h
mov ah, 09h   
mov dx, Z 
int 21h 
mov ax, 4C00h 
int 21h 
 
END START
