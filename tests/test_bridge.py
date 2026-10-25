import sys
import time
import unittest
from pathlib import Path
from unittest.mock import patch, Mock
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'bridge'))
from server import create_app

TOKEN='test-token-that-is-at-least-24-characters'
class BridgeTests(unittest.TestCase):
 def setUp(self):
  self.config={'token':TOKEN,'latitude':None,'longitude':None,'rss_urls':[], 'home_entities':['light.desk'], 'controllable_entities':['light.desk'], 'home_assistant_url':'http://ha.local','home_assistant_token':'test'}
  self.client=create_app(self.config).test_client();self.auth={'Authorization':'Bearer '+TOKEN}
 def test_auth_all_routes(self):
  for path in ['/feed','/health','/voice','/toggle']:
   response=self.client.post(path) if path in ['/voice','/toggle'] else self.client.get(path)
   self.assertEqual(response.status_code,401)
 def test_reject_default_token(self):
  with self.assertRaises(ValueError):create_app({'token':'REPLACE_WITH_A_RANDOM_TOKEN_AT_LEAST_24_CHARACTERS'})
 def test_disallow_controls(self):
  with patch('server.requests.post') as post:
   for entity in ['lock.front','light.unknown','switch.heater']:
    self.assertEqual(self.client.post('/toggle',headers=self.auth,json={'entity_id':entity}).status_code,403)
   post.assert_not_called()
 def test_allowed_toggle(self):
  with patch('server.requests.post', return_value=Mock()) as post:
   self.assertEqual(self.client.post('/toggle',headers=self.auth,json={'entity_id':'light.desk'}).status_code,200)
   self.assertEqual(post.call_args.kwargs['json'],{'entity_id':'light.desk'})
 def test_voice_without_model(self):
  self.assertEqual(self.client.post('/voice',headers=self.auth,data=b'RIFF').status_code,503)
 def test_invalid_voice(self):
  cfg=dict(self.config,transcription_model='tiny.en');client=create_app(cfg).test_client()
  self.assertEqual(client.post('/voice',headers=self.auth,data=b'bad audio').status_code,400)
 def test_recurring_calendar_and_rss(self):
  from datetime import datetime,timezone,timedelta
  today=datetime.now(timezone.utc)+timedelta(hours=1)
  stamp=today.strftime('%Y%m%dT%H%M%SZ')
  end=(today+timedelta(hours=1)).strftime('%Y%m%dT%H%M%SZ')
  ics=f'BEGIN:VCALENDAR\r\nVERSION:2.0\r\nBEGIN:VEVENT\r\nUID:1\r\nDTSTART:{stamp}\r\nDTEND:{end}\r\nRRULE:FREQ=DAILY;COUNT=3\r\nSUMMARY:Test appointment\r\nEND:VEVENT\r\nEND:VCALENDAR\r\n'.encode()
  cfg=dict(self.config,calendar_ics_url='https://calendar.test/feed',rss_urls=['https://news.test/feed'],home_assistant_url='')
  client=create_app(cfg).test_client()
  def get(url,**kwargs):
   r=Mock();r.content=ics if 'calendar' in url else b'<rss version="2.0"><channel><title>Test</title><item><title>A useful headline</title></item></channel></rss>';return r
  with patch('server.requests.get',side_effect=get):
   for _ in range(100):
    data=client.get('/feed',headers=self.auth).get_json()
    if not data['refreshing']:break
    time.sleep(.01)
   self.assertIn('Test appointment',data['calendar']);self.assertIn('A useful headline',data['briefing'])
 def test_feed_outage_is_explicit(self):
  cfg=dict(self.config,latitude=53,longitude=-113,home_assistant_url='');client=create_app(cfg).test_client()
  with patch('server.requests.get',side_effect=ConnectionError):
   for _ in range(100):
    data=client.get('/feed',headers=self.auth).get_json()
    if not data['refreshing']:break
    time.sleep(.01)
   self.assertIn('unavailable',data['weather']);self.assertIn('weather',data['errors'])
if __name__=='__main__':unittest.main()
