import numpy as np
import scipy
import matplotlib.pyplot as plt

samples = np.fromfile("data/capture.dat", dtype=np.float32)
print(len(samples))
samples -= np.mean(samples)
plt.plot(samples)

fs_Hz = 44100
chirp_rate_Hz_per_sec = 10e3
f0_Hz = 0
f1_Hz = 3e3

d = (f1_Hz - f0_Hz) / chirp_rate_Hz_per_sec
t = np.arange(0, d, 1 / fs_Hz)
p = f0_Hz * t + 0.5 * chirp_rate_Hz_per_sec * t ** 2
x = np.exp(2j * np.pi * p)

y0 = scipy.signal.correlate(samples, x, mode="full", method="fft")
y0 = np.abs(y0)

y1 = np.convolve(samples, np.flip(x), mode="full")
y1 = np.abs(y1)

n = len(samples) + len(x) - 1
n1 = 2 ** int(np.log2(n) + 1)

xt = np.fft.fft(x, n1)
samplest = np.fft.fft(samples, n1)
xt = np.conj(xt)

yt = samplest * xt
y2 = np.fft.ifft(yt)
y2 = np.abs(y2)

plt.plot(y0 / max(y0))
plt.plot(y1 / max(y1))
plt.plot(y2 / max(y2))

plt.show()
