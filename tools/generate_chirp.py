import numpy as np
import scipy.io.wavfile as wavfile
import matplotlib.pyplot as plt
import scipy 

fs = 44100
rate = 10e3
f0 = 0
f1 = 3e3
d = (f1 - f0) / rate

t = np.arange(0, d, 1 / fs)
p = f0 * t + 0.5 * rate * t ** 2
x = np.cos(2 * np.pi * p)

x *= scipy.signal.windows.hann(len(x))

m = 2
m = int(m * fs)
xm = np.zeros(m)

x = np.concatenate((xm, x, xm))
t = np.arange(len(x)) / fs

x += 1 * (2 * np.random.rand(len(x)) - 1)

print(len(x))

plt.plot(t, x)
plt.show()

x.astype(np.float32).tofile("data/generated.dat")