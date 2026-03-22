import numpy as np
import matplotlib.pyplot as plt

signal = np.fromfile("data/generated.dat", np.dtype(np.float32))
output = np.fromfile("data/out.dat", np.dtype(np.float32))
cfar = np.fromfile("data/cfar.dat", np.dtype(np.float32))
guess = np.fromfile("data/guess.dat", np.dtype(np.float32))

gi = guess[0::2]
gg = guess[1::2]
print(gi)

# output = output[:-160]
# cfar = cfar[160:]
print(len(output), len(cfar))

t = np.arange(len(output))
plt.plot(t + 160, output, label="output")
plt.plot(t, cfar, label="cfar")

# i = np.argwhere(output > cfar)
# plt.scatter(i, output[i], marker="x", color="red")
# print(gi, gg)
gi += 1
plt.scatter(gi, gg, marker="x", color="green")

plt.legend()
plt.show()