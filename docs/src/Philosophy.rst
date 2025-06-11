****************
Philosophy
****************


Fast code with a simple interface
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Aare should be fast and efficient, but also easy to use. We strive to keep a simple interface that feels intuitive. 
Internally we use C++ for performance and the ability to integrate the library in other programs, but we see most 
users using the Python interface. 

Live at head
~~~~~~~~~~~~~~~~~~

As a user of the library you should be able to, and is expected to, use the latest version. Bug fixes will rarely be backported
to older releases. By upgrading frequently you will benefit from the latest features and minimize the effort to maintain your scripts/code 
by doing several small upgrades instead of one big upgrade.

API
~~~~~~~~~~~~~~~~~~

We aim to keep the API stable and only break it for good reasons. But specially now in the early stages of development
the API will change. On those occasions it will be clearly stated in the release notes. However, the norm should be a 
backward compatible API.

Documentation
~~~~~~~~~~~~~~~~~~

Being a library it is important to have a well documented API. We use Doxygen to generate the C++ documentation
and Sphinx for the Python part. Breathe is used to integrate the two into one Sphinx html site. The documentation is built
automatically on release by the CI and published to GitHub pages. In addition to the generated API documentation,
certain classes might need more descriptions of the usage. This is then placed in the .rst files in the docs/src directory.

.. attention::

    The code should be well documented, but using descriptive names is more important. In the same spirit
    if a function is called `getNumberOfFrames()` you don't need to write a comment saying that it gets the 
    number of frames.


Dependencies
~~~~~~~~~~~~~~~~~~

Deployment in the scientific community is often tricky. Either due to old OS versions or the lack of package managers. 
We strive to keep the dependencies to a minimum and will vendor some libraries to simplify deployment even though it comes
at a cost of build time.