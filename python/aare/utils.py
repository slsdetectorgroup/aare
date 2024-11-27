import numpy as np

def random_pixels(n_pixels, xmin=0, xmax=512, ymin=0, ymax=1024):
    """Return a list of random pixels.
    
    Args:
        n_pixels (int): Number of pixels to return.
        rows (int): Number of rows in the image.
        cols (int): Number of columns in the image.
        
    Returns:
        list: List of (row, col) tuples.
    """
    return [(np.random.randint(ymin, ymax), np.random.randint(xmin, xmax)) for _ in range(n_pixels)]


def random_pixel(xmin=0, xmax=512, ymin=0, ymax=1024):
    """Return a random pixel.
    
    Returns:
        tuple: (row, col)
    """
    return random_pixels(1, xmin, xmax, ymin, ymax)[0]