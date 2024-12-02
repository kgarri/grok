import time

def fib(n) :
    if n<=1: 
        return 1

    return fib(n-1) + fib(n-2)


start = time.time()

fib(15)

end = time.time() - start

print(end *  pow(10, 3))
