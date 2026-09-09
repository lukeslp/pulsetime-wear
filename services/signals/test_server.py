import unittest
import json
import threading
from urllib.request import Request, urlopen
from urllib.error import HTTPError
from unittest.mock import patch
from server import adapt_feed, classify_host, validate_config, Bridge, BoundedServer, handler


class SignalsTest(unittest.TestCase):
    def test_busy_is_not_failed(self):
        self.assertEqual(classify_host([(True,.1)], 1.5,.4,.4)[0], 'busy')
        self.assertEqual(classify_host([(True,.1)], .1,.4,.4)[0], 'healthy')
        self.assertEqual(classify_host([(False,.1),(True,.1)], .1,.4,.4)[0], 'degraded')
        self.assertEqual(classify_host([(True,.1)], .1,.02,.4)[0], 'degraded')
        state, detail = classify_host([(False,3)], .1,.4,.4)
        self.assertEqual(state, 'down'); self.assertIn('host agent is reachable', detail)
        self.assertEqual(classify_host([], 0,.4,.4)[0], 'unknown')

    def test_preserve_observation_time(self):
        cfg = dict(label='Door',observed_path='at',value_path='door',states={'closed':'healthy','open':'degraded'},ttl=300)
        result=adapt_feed(dict(at=1000,door='open'),cfg,now=1100)
        self.assertEqual(result['observed_at'],1000);self.assertEqual(result['expires_at'],1300)
        self.assertEqual(result['state'],'degraded')
        for timestamp in (True,900,1201,None):
            with self.assertRaises(ValueError):adapt_feed(dict(at=timestamp,door='open'),cfg,now=1200)

    def test_threshold_feed(self):
        cfg=dict(label='Tide',observed_path='at',value_path='samples',thresholds=[1,2,3])
        self.assertEqual(adapt_feed(dict(at=1000,samples=[0,1.5,4]),cfg,now=1050)['values'],[0,1,3])
        for values in ([0,1], [None,1,2], [float('nan'),1,2]):
            with self.assertRaises(ValueError):adapt_feed(dict(at=1000,samples=values),cfg,now=1050)

    def test_failed_source_is_unknown(self):
        bridge=Bridge({'checks':[]})
        with patch('server.host_reading',side_effect=OSError):
            self.assertEqual(bridge.fetch('/v1/status')['state'],'unknown')
        self.assertIsNone(bridge.fetch('/v1/feeds/not-configured'))

    def test_configuration_is_not_a_request_proxy(self):
        with self.assertRaises(ValueError):validate_config({'feeds':{'x':{'url':'http://example.com','label':'x'}}})
        with self.assertRaises(ValueError):validate_config({'checks':['file:///etc/passwd']})

    def test_http_auth_and_unknown_path(self):
        bridge=Bridge({'checks':[]})
        service=BoundedServer(('127.0.0.1',0),handler(bridge,'test-key'*4))
        thread=threading.Thread(target=service.serve_forever,daemon=True);thread.start()
        url=f'http://127.0.0.1:{service.server_port}'
        try:
            with urlopen(url+'/health') as r:self.assertTrue(json.load(r)['ok'])
            with self.assertRaises(HTTPError) as rejected:urlopen(url+'/v1/status')
            self.assertEqual(rejected.exception.code,401)
            rejected.exception.close()
            request=Request(url+'/v1/status',headers={'Authorization':'Bearer '+'test-key'*4})
            with patch('server.host_reading',return_value={'state':'unknown'}), urlopen(request) as r:
                self.assertEqual(json.load(r)['state'],'unknown');self.assertEqual(r.headers['Cache-Control'],'no-store')
            with self.assertRaises(HTTPError) as missing:
                urlopen(Request(url+'/v1/feeds/missing',headers={'Authorization':'Bearer '+'test-key'*4}))
            self.assertEqual(missing.exception.code,404)
            missing.exception.close()
        finally:
            service.shutdown();service.server_close();thread.join()


if __name__ == '__main__':unittest.main()
