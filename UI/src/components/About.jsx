import Link from '@mui/material/Link';
import Typography from '@mui/material/Typography';

export const About = () => {
  return (
    <>
      <Typography variant='h3'>About</Typography>
      <Typography variant='body1'>
        Made with {'<3'} by p0px.<br/>
        <Link href='https://gitlab.com/p0px/wabbt'>Project Link</Link>
      </Typography>
    </>
  );
};

export default About;
