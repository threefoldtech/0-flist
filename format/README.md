# Generate C files from model

Require `c-capnproto` and `capnproto`

```
capnp compile -oc flist.capnp
cp flist.capnp.* ../libflist/
```
