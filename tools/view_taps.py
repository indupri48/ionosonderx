import numpy as np
import matplotlib.pyplot as plt

a = np.fromfile("taps.dat", np.dtype(np.complex64))
plt.plot(a.real)
plt.plot(a.imag)
plt.show()