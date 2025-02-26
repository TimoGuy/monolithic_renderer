import argparse
import os

parser = argparse.ArgumentParser(description='Settings editor for geometry material.')
parser.add_argument('name', type=str,
                    help='Name of geometry material to look for.')
args = parser.parse_args()

# Main.
if __name__ == '__main__':
    files = [f for f in os.listdir('.') if os.path.isfile(f)]
    for f in files:
        if is_geom_mat_file_pair(f, files):
            print('found!')
            break


#
def is_geom_mat_file_pair(fname: str, all_files) -> bool:
    if not is_geom_mat_file(fname):
        return False
    
    # @TODO: START HERE!~!!!

#
def is_geom_mat_file(fname: str) -> bool:
    tokens = get_fname_tokens(fname)

    if len(tokens) != 4:
        return False
    if tokens[0] != 'gm':
        return False
    if tokens[3] not in ['vert', 'frag']:
        return False
    return True


#
def get_fname_tokens(fname: str):
    return fname.split('.')
