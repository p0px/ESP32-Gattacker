import { useNavigate } from 'react-router-dom';

import { Menu } from './common';

const bleAttacks = [{
  id: 'nspam',
  name: 'Name Spam',
  class: 'offensive',
  color: 'error',
  desc: `
  Sends advertisments usings
  random MAC addresses. This
  will appear in your phones
  bluetooth when going to pair.`
}, {
  id: 'gatk',
  name: 'Gatt Attack',
  class: 'offensive',
  color: 'error',
  desc: `
  Spoofs a BLE device completely.
  Will clone its MAC and GATT services
  and advertise at a faster interval tricking
  devices to connect.
  `
}, {
  id: 'fpspam',
  name: 'FastPair Spam',
  class: 'offensive',
  color: 'error',
  desc: `
  Sends Google FastPair BLE
  advertisements that can cause
  popups on some phones.
`
}, {
  id: 'esspam',
  opt: 10,
  name: 'EasySetup Spam',
  class: 'offensive',
  color: 'error',
  desc: `
  Sends Samsung EasySetup
  advertisements that can
  cause popups on some phones.
`
}, {
  id: 'blespamd',
  name: 'Spam Detect',
  class: 'defensive',
  color: 'primary',
  desc: `
  Detects various BLE spam
  advertisement attacks that
  are known to cause disruptive
  actions and unwanted popups.
`
}, {
  id: 'vhci',
  opt: 9,
  name: 'vHCI',
  class: 'tool',
  color: 'primary',
  desc: `
  Virtual HCI for the
  Bluetooth controller.
`
}, {
  id: 'bleskimd',
  opt: 12,
  name: 'Skimmer Detect',
  class: 'defensive',
  color: 'primary',
  desc: `
  Detects potential
  Gas Station Pump
  skimmers by looking
  for specific advertisements.
`
}, {
  id: 'drone',
  opt: 13,
  name: 'Fake Drone',
  class: 'defensive',
  color: 'error',
  desc: `
  Sends OpenDroneID BLE
  advertisements to create
  a fake drone that flies
  around given coords.
`}, {
  id: 'smarttag',
  opt: 14,
  name: 'Smart Tagger',
  class: 'defensive',
  color: 'primary',
  desc: `
  Detects smart tags
  such as AirTags and
  Tiles. It will attempt
  to connect and set
  off an audio alert
  on the device.`
}];

export const Ble = () => {
  const navigate = useNavigate();

  const onCardClick = (i) => {
    navigate(`/ble/${i.id}`);
  };

  return (
    <Menu
      apps={bleAttacks}
      name={'BLE'}
      desc={'Tools for Bluetooth Low Energy'}
      onClick={onCardClick}
    />
  );
};

export default Ble;
