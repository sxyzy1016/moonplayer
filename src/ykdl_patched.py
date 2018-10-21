#!/usr/bin/env python


# This is a runtime patcher
# Patch ykdl at runtime to make it provide enouth message for MoonPlayer


# Change default coding
import os, sys, json, platform, io
reload(sys)
sys.setdefaultencoding('UTF8')


# Init environment and import module
if platform.system() == 'Darwin':
    _srcdir = '%s/Library/Application Support/MoonPlayer/ykdl/' % os.getenv('HOME')
else:
    _srcdir = '%s/.moonplayer/ykdl/' % os.getenv('HOME')
_filepath = os.path.dirname(sys.argv[0])
sys.path.insert(0, _srcdir)


# List of unseekable video streams
unseekables = ('Sohu.com',)

# Patch bilibase
danmaku_url = ''
from ykdl.extractors.bilibili.bilibase import BiliBase
old_bilibase_prepare = BiliBase.prepare
def bilibase_prepare(self):
    retVal = old_bilibase_prepare(self)
    global danmaku_url
    danmaku_url = 'http://comment.bilibili.com/{}.xml'.format(self.vid)
    return retVal
BiliBase.prepare = bilibase_prepare

# Patch jsonlize
from ykdl.videoinfo import VideoInfo
old_jsonlize = VideoInfo.jsonlize
def jsonlize(self):
    retVal = old_jsonlize(self)
    retVal['danmaku_url'] = danmaku_url
    return retVal
VideoInfo.jsonlize = jsonlize

# Run ykdl
import cykdl.__main__
if __name__ == '__main__':
    cykdl.__main__.main()