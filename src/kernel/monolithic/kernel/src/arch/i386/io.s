.global outb
.global inb

/* outb - sends a byte to an I/O port
   Stack: [esp + 8] is the data byte
          [esp + 4] is the I/O port */
outb:
    /* movb: 'b' suffix indicates a byte-sized operation
       Source: 8(%esp) -> Destination: %al */
    movb 8(%esp), %al

    /* movw: 'w' suffix indicates a word-sized (16-bit) operation
       Source: 4(%esp) -> Destination: %dx */
    movw 4(%esp), %dx

    /* outb: sends the byte in %al to the port in %dx */
    outb %al, %dx
    ret

/* inb - returns a byte from the given I/O port
   Stack: [esp + 4] is the I/O port */
inb:
    /* Move the 16-bit port address into the DX register */
    movw 4(%esp), %dx

    /* inb: reads a byte from port %dx into %al */
    inb %dx, %al

    /* The return value is already in %al, which C expects */
    ret