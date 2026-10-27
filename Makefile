compile: parser.c scanner.c driver.c ast.c ast-print.c codegen.c scanner.h ast.h symbol.h threeaddr.h codegen.h
	gcc -Wall -g parser.c scanner.c driver.c ast.c ast-print.c codegen.c -o compile

clean:
	rm -f compile *.o