from datetime import datetime
import numpy as np

# Parameters to choose
conf_file_name = 'gd_config'
wall_value = 42
target_value = 10

configuration_grid = np.zeros((10, 20))

def make_walls(grid):
    grid[:, 0:1] = wall_value
    grid[:, -1:] = wall_value
    grid[0:1, :] = wall_value
    grid[-1:, :] = wall_value
    return grid


if __name__ == "__main__":
    # setting up the configuration
    # walls
    configuration_grid = make_walls(configuration_grid)
    # setting the target
    configuration_grid[6:7, 6:7] = target_value

    print(configuration_grid)

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

                # needed for structure
                print('', file=f)