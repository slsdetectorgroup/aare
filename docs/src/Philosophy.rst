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

As a user of the library you should be able to, and is expected to use the latest version. Bug fixes will rarely be backported
to older releases. By upgrading frequently you will benefit from the latest features and minimize the effort due so changes 
in the API. 


API
~~~~~~~~~~~~~~~~~~

We aim to keep the API stable and only break it for good reasons. But specially now in the early stages of development
the API will change. On those occasions it will be clearly stated in the release notes. However, the norm should be a 
backward compatible API.


Dependencies
~~~~~~~~~~~~~~~~~~

Deployment in the scientific community is often tricky. Either due to old OS versions or the lack of package managers. 
We strive to keep the dependencies to a minimum and will vendor some libraries to simplify deployment even though it comes
at a cost of build time.