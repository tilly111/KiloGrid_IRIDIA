from datetime import datetime
import numpy as np

# Parameters to choose
conf_file_name = 'test_config'
wall_value = 42
target_value = 10

configuration_grid = np.zeros((20, 40))

def make_walls(grid):
    grid[:, 0:2] = wall_value
    grid[:, -2:] = wall_value
    grid[0:2, :] = wall_value
    grid[-2:, :] = wall_value
    return grid


if __name__ == "__main__":
    # setting up the configuration
    # walls
    #configuration_grid = make_walls(configuration_grid)
    # setting the target
    #configuration_grid[12:14, 12:14] = target_value

    #print(configuration_grid)

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
                for i in range(4):
                    print(hex(i), file=f)

                # needed for structure
                print('', file=f)