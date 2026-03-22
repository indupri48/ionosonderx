import sys

import numpy as np
import matplotlib.pyplot as plt

filename = sys.argv[1]

x = np.fromfile(filename, np.dtype(np.float32))

plt.plot(x)
plt.show()