macro(warn_unmaintained name)
  message(WARNING "[unmaintained] ${name} is unmaintained!")
  message(WARNING "[unmaintained] use at your own risk!")
  message(
    WARNING
      "[unmaintained] If problems occur please check https://github.com/FreeRDP/FreeRDP/issues for known issues, but be prepared to fix them on your own!"
  )
  message(WARNING "[unmaintained] Developers hang out in https://matrix.to/#/#FreeRDP:matrix.org?via=matrix.org")
  message(
    WARNING
      "[unmaintained] - dont hesitate to ask some questions. (replies might take some time depending on your timezone)"
  )
  message(WARNING "[unmaintained] - if you intend using this component write us a message")
endmacro()
