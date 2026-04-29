.MODEL SMALL 
.STACK 100h 
 
.CODE                
START: 
 
mov ah, 02h      
mov dl, 'B'      
int 21h          
 
mov ax, 4C00h    
int 21h 
END START
