****************
Numpy arrays
****************

Most Python functions in aare take NumPy arrays as input. The bindings in
``python/src/`` receive them as ``py::array_t<T>`` and wrap the buffer in an
``NDView`` before calling the C++ implementation. No data is copied, so the
C++ code reads and writes the memory that the caller's array owns. This page
explains the choices we make when declaring such an argument and what they
mean for the user.

The preferred pattern
~~~~~~~~~~~~~~~~~~~~~~~~

Declare the argument with only the element type, take it by value, mark it
``noconvert`` in the ``py::arg`` list and let ``make_view`` check the rest:

.. code-block:: cpp

    m.def(
        "push",
        [](Pedestal &self, py::array_t<uint16_t> frame) {
            auto view = make_view_2d(frame);
            self.push(view);
        },
        py::arg("frame").noconvert());

The responsibilities are split between pybind11 and ``make_view``:

- pybind11 checks that the argument is a NumPy array with exactly the
  requested dtype. Anything else fails with a ``TypeError``, and since the
  argument is ``noconvert`` pybind11 never tries to build a matching array
  from it.
- ``make_view`` (``python/src/np_helper.hpp``) checks that the array has the
  expected number of dimensions and is C-contiguous. It raises a
  ``ValueError`` with a message that tells the user what to fix, for example
  ``Array is not C-contiguous. Use np.ascontiguousarray(arr)``. Writing
  functions use ``make_view`` which also rejects read-only arrays. Read-only
  input goes through ``make_const_view``.

The result is a binding that either operates on the caller's memory or
refuses the call. It never silently works on a temporary copy.

Take the array by value
~~~~~~~~~~~~~~~~~~~~~~~~~~

``py::array_t`` is a handle to the Python object, not the data. Passing it by
value copies the handle, which increments the reference count of the NumPy
array, and nothing else. The pixel data stays where it is. We therefore
declare the parameter as ``py::array_t<T> frame`` rather than
``py::array_t<T> &frame``:

- A reference saves one reference-count increment per call, which is not
  measurable next to the work done on a frame.
- A by-value parameter is a plain lvalue inside the lambda, so
  ``make_view``, which calls ``mutable_data()``, works without further
  qualification. A ``const py::array_t<T> &`` does not compile with it.
- It matches the usual pybind11 style for ``py::object`` and its subclasses,
  so the binding reads the same as examples in the pybind11 documentation.

Why not let pybind11 convert
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Without ``noconvert``, pybind11 tries every overload once with conversions
disabled and then once more with conversions enabled. In the second pass a
``py::array_t<T>`` argument accepts anything NumPy can turn into an array of
type ``T``: an array with another dtype, a non-contiguous array, or even a
Python list. pybind11 creates a new array, hands the converted copy to the
binding and drops it when the call returns. For detector data this has
several consequences:

- The copy costs time and memory, and the user gets no indication that it
  happened. For a stack of frames this can be gigabytes.
- Writes are lost. A function that fills or updates its input, for example
  a pedestal subtraction in place, writes into the temporary copy.
- Values are cast without warning. ``forcecast`` allows unsafe casts, so a
  ``float64`` frame is truncated to ``uint16`` instead of being rejected.
- Overloads stop working as intended. ``apply_calibration`` is registered for
  both ``double`` and ``float``. With conversions allowed the first overload
  would accept everything by casting and the ``float`` overload would never be
  chosen.

Argument conversion is only worth it for small inputs where convenience beats
control, for example the parameter vector passed to a fit. Even then the
function must not write into the argument.

.. note::

    When a binding intentionally lets pybind11 convert, say so with a comment
    next to the argument. Otherwise a missing ``noconvert`` looks like an
    oversight and the next person to touch the binding will add it.

    .. code-block:: cpp

        // Conversion allowed: small read-only vector, accept lists and any dtype
        py::array_t<double, py::array::c_style | py::array::forcecast> par

What the template flags mean
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

``py::array_t<T, Flags>`` takes an optional second template argument. The
flags do two things: they add requirements that the array must fulfil for the
*non-converting* check to pass, and they tell NumPy what to produce when a
conversion *is* allowed.

``py::array::c_style``
    The array must be C-contiguous. With ``noconvert`` a non-contiguous array
    is rejected by pybind11 with a ``TypeError`` listing the accepted
    signatures. Without ``noconvert`` pybind11 makes a contiguous copy.
    Since ``make_view`` already enforces contiguity and gives a more helpful
    ``ValueError``, the flag is redundant in the preferred pattern. It is
    still present in some older bindings and is harmless there.

``py::array::f_style``
    Same as above for Fortran order. ``NDView`` assumes C order, so we do not
    use it.

``py::array::forcecast``
    Allows NumPy to perform unsafe casts, such as ``float64`` to ``uint16``,
    when converting. It is the **default** value of the template argument, so
    ``py::array_t<uint16_t>`` and ``py::array_t<uint16_t, py::array::forcecast>``
    are the same type. The flag has no effect together with ``noconvert``
    because no conversion takes place. It only matters when conversions are
    allowed, where it decides whether a lossy cast is accepted or the call is
    rejected.

The common spelling ``py::array_t<T, py::array::c_style | py::array::forcecast>``
therefore means: when converting, produce a contiguous array of type ``T`` and
cast whatever you were given. Combined with ``noconvert`` it collapses to
"C-contiguous array of exactly type ``T``", which is what the plain
``py::array_t<T>`` plus ``make_view`` already gives us.

Summary
~~~~~~~~~~~~~~~~~~

- Declare the argument as ``py::array_t<T>`` by value and add
  ``.noconvert()`` to its ``py::arg``. The handle is reference counted, so
  passing it by value does not copy the data.
- Create the view with ``make_view_1d/2d/3d`` or ``make_const_view_*`` and
  let it validate dimensions and contiguity. Add an explicit check only when
  the binding accepts several shapes, as ``apply_calibration`` does for 2D
  and 3D pedestals.
- Return data to Python with ``return_image_data`` or ``return_vector`` so that
  the NumPy array owns the C++ allocation.
- Only allow conversion for small, read-only inputs where accepting lists or
  other dtypes is a real convenience for the user, and mark it with a comment
  in the code.
