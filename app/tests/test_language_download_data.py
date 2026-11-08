"""Downloader failure contracts; no network, tools or native processes."""
import hashlib
import http.client
import importlib.util
import io
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
import urllib.error

spec=importlib.util.spec_from_file_location("language_setup",Path(__file__).resolve().parents[1]/"scripts/setup_languages.py")
setup=importlib.util.module_from_spec(spec);spec.loader.exec_module(setup)

class Downloads(unittest.TestCase):
    def setUp(self):
        self.temporary=tempfile.TemporaryDirectory()
        self.root=Path(self.temporary.name)
        self.root_patch=patch.object(setup,"ROOT",self.root);self.root_patch.start()
        self.data=b"verified archive bytes"
        self.item={"url":"https://example.invalid/archive.tar.gz","sha256":hashlib.sha256(self.data).hexdigest()}
    def tearDown(self):
        self.root_patch.stop();self.temporary.cleanup()
    def fetch(self,effects):
        return patch.object(setup.urllib.request,"urlopen",side_effect=effects)
    def test_verified_cache_needs_no_network(self):
        with self.fetch([io.BytesIO(self.data)]) as request:
            result=setup.archive(self.item)
        self.assertEqual(result.read_bytes(),self.data)
        with self.fetch([AssertionError("network used for verified cache")]):
            self.assertEqual(setup.archive(self.item),result)
        self.assertEqual(request.call_count,1)
    def test_transient_timeout_uses_new_staging(self):
        with self.fetch([TimeoutError("read timeout"),io.BytesIO(self.data)]) as request,patch.object(setup.time,"sleep") as sleep:
            result=setup.archive(self.item)
        self.assertEqual(result.read_bytes(),self.data)
        self.assertEqual(request.call_count,2);sleep.assert_called_once_with(1)
        self.assertEqual(len(list(result.parent.glob("*.partial"))),1)
    def test_checksum_mismatch_never_retried_or_published(self):
        with self.fetch([io.BytesIO(b"wrong bytes")]) as request,patch.object(setup.time,"sleep") as sleep:
            with self.assertRaisesRegex(RuntimeError,"checksum mismatch"):
                setup.archive(self.item)
        self.assertEqual(request.call_count,1);sleep.assert_not_called()
        self.assertFalse(list((self.root/"runtime/tools/downloads").glob("*.tar.gz")))
    def test_http_not_found_is_not_transient(self):
        error=urllib.error.HTTPError(self.item["url"],404,"Not found",None,None)
        with self.fetch([error]) as request,patch.object(setup.time,"sleep") as sleep:
            with self.assertRaises(urllib.error.HTTPError):setup.archive(self.item)
        self.assertEqual(request.call_count,1);sleep.assert_not_called()
    def test_transient_failures_are_bounded(self):
        with self.fetch([TimeoutError("read timeout")]*3) as request,patch.object(setup.time,"sleep") as sleep:
            with self.assertRaises(TimeoutError):setup.archive(self.item)
        self.assertEqual(request.call_count,3);self.assertEqual(sleep.call_count,2)
        self.assertEqual(len(list((self.root/"runtime/tools/downloads").glob("*.partial"))),3)
    def test_incomplete_response_is_retryable(self):
        with self.fetch([http.client.IncompleteRead(b"short",30),io.BytesIO(self.data)]),patch.object(setup.time,"sleep"):
            self.assertEqual(setup.archive(self.item).read_bytes(),self.data)

if __name__=="__main__":unittest.main()
