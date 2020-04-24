# MacOS Deployment

The `macdeployqtplus` script should not be run manually. Instead, after building as usual:

```bash
ninja osx-dmg
```

During the deployment process, the disk image window will pop up briefly
when the fancy settings are applied. This is normal, please do not interfere,
the process will unmount the DMG and cleanup before finishing.

When complete, it will have produced `Bitcoin-Cash-Node.dmg`.
