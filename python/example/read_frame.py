from aare import File, Frame

if __name__ == "__main__":
    file = File("/home/bb/github/aare/data/jungfrau_single_master_0.json")
    frame = file.get_frame(0)
    print(frame.rows, frame.cols)
    print(frame.get(0,0))