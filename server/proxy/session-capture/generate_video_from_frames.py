import os
import argparse
import time
import cv2
from os.path import join, getmtime

def get_image_size(path):
    img = cv2.imread(path)
    height, width, _ = img.shape
    return (width, height)

def generate_video(output, frames, size, fps):
    out = cv2.VideoWriter(
        args.output, cv2.VideoWriter_fourcc(*'DIVX'), args.fps, size)

    for frame in frames:
        img = cv2.imread(frame)
        out.write(img)

    out.release()

def main(args):
    # Load input frames, sorted by creation time.
    files = [join(args.input, f) for f in os.listdir(
        args.input) if os.path.isfile(join(args.input, f))]
    files.sort(key=lambda x: getmtime(x))

    print('Generating video...')
    print(f'Frame count: {len(files)}')

    start = time.time()
    generate_video(args.output, files, get_image_size(files[0]), args.fps)

    print(
        f'Output file {args.output} generated in {time.time() - start} seconds.')


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "-i", "--input", help="path to a directory containing all frames")
    parser.add_argument(
        "-o", "--output", help="avi output file path", default="video.avi")
    parser.add_argument("-f", "--fps", type=int, help="frames per second", default=8)
    args = parser.parse_args()

    main(args)
