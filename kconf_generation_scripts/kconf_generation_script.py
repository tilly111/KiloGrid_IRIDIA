from datetime import datetime
import numpy as np
import imageio
import matplotlib.pyplot as plt
#import visvis as vv
import cv2 as cv

# Parameters to choose
conf_file_name = 'christmas_final'
wall_value = 42
target_value = 10

configuration_grid = np.zeros((10, 20))


def make_walls(grid):
    grid[:, 0:1] = wall_value
    grid[:, -1:] = wall_value
    grid[0:1, :] = wall_value
    grid[-1:, :] = wall_value
    return grid

def make_tree(grid):
    begin = [4, 3, 2, 4, 3, 2, 4, 3, 2, 4, 3, 2, 4, 4, 4, 4, 4, 4]
    end =   [6, 7, 8, 6, 7, 8, 6, 7, 8, 6, 7, 8, 6, 6, 6, 6, 6, 6]
    for y in range(16):
        if y < 12:
            grid[begin[y]:end[y], 17-y] = np.ones((end[y]-begin[y],)) * 24
        else:
            grid[begin[y]:end[y], 17 - y] = np.ones((end[y] - begin[y],)) * 25

    return grid

if __name__ == "__main__":
    # setting up the configuration
    # walls
    configuration_grid = make_walls(configuration_grid)
    # setting the target
    #configuration_grid[6:7, 6:7] = target_value
    #configuration_grid = make_tree(configuration_grid)
    #for y in range(20):
    #    print(configuration_grid[:, y])

    #print(configuration_grid_manual.shape)
    #im = imageio.imread('pixil-frame-0.png')
    im = cv.imread("pixil-frame-0-2.png")
    gray = cv.cvtColor(im, cv.COLOR_BGR2GRAY)
    gray = gray.transpose()
    gray = np.fliplr(gray)
    gray[gray == 99] = 3
    gray[gray == 87] = 1
    gray[gray == 135] = 2
    gray[gray == 94] = 4
    gray[gray == 224] = 5
    #im = plt.imread('pixil-frame-0.png')
    print(gray)

    print(gray.shape)
    #matrix = np.array(im)
    #print(matrix.shape)



    with open(conf_file_name + '.kconf', 'w') as f:
        # meta data TODO
        now = datetime.now()
        dt_string = now.strftime("%d/%m/%Y %H:%M:%S")
        print('# Experiment: ', conf_file_name, file=f)
        print('# Generation Time: ', dt_string, file=f)
        print('\n', file=f)

        # writing for each module
        for x in range(10):
            for y in range(20):
                # print header
                print('address', file=f)
                print('module:' + str(x) + '-' + str(y) + '\n', file=f)
                print('data', file=f)
                # fill with values ..
                print(hex(x), file=f)
                print(hex(y), file=f)
                print(hex(int(configuration_grid[x, y])), file=f)
                print(hex(gray[x * 2, y * 2 + 1]), file=f)
                print(hex(gray[x * 2 + 1, y * 2 + 1]), file=f)
                print(hex(gray[x * 2, y * 2]), file=f)
                print(hex(gray[x * 2 + 1, y * 2]), file=f)

                # needed for structure
                print('', file=f)